#include "lora.h"
#define LORA_ADDR E220::BROADCAST

namespace component {

LoRa::LoRa(pin_t aux, pin_t m0, pin_t m1, pin_t tx, pin_t rx, uint8_t channel, unsigned number)
  : process::Component("LoRa", component_id_base + number),
    lora_serial_(1),  // HardwareSerial(1)
    e220_(lora_serial_, aux, m0, m1),
    antenna_switch_(false),
    tx_pin_(tx),
    rx_pin_(rx),
    aux_pin_(aux),
    m0_pin_(m0),
    m1_pin_(m1),
    channel_(channel) {
}

LoRa::LoRa(pin_t aux, pin_t m0, pin_t m1, pin_t antenna_A, pin_t antenna_B, pin_t tx, pin_t rx, uint8_t channel, unsigned number)
  : process::Component("LoRa", component_id_base + number),
    lora_serial_(1),  // HardwareSerial(1)
    e220_(lora_serial_, aux, m0, m1),
    antenna_switch_(true),
    antenna_A_(antenna_A),
    antenna_B_(antenna_B),
    tx_pin_(tx),
    rx_pin_(rx),
    aux_pin_(aux),
    m0_pin_(m0),
    m1_pin_(m1),
    channel_(channel) {
}

void LoRa::setup() {
  if (!tx_mutex_) {
    LOG("LoRa setup error: mutex allocation failed");
    return;
  }
  if (antenna_switch_) {
    pinMode(antenna_A_, OUTPUT);
    pinMode(antenna_B_, OUTPUT);
  }
  
  lora_serial_.begin(9600, SERIAL_8N1, rx_pin_, tx_pin_);

  bool ok = e220_.begin();
  delay(1000);
  ok &= e220_.setMode(E220::Mode::CONFIG_DS);
  ok &= e220_.setParametersToDefault();
  ok &= e220_.setSerialBaudRate(115200);
  ok &= e220_.setDataRate(E220::SF::SF9, E220::BW::BW125kHz);
  ok &= e220_.setEnvRSSIEnable(true);
  ok &= e220_.setSendMode(E220::SendMode::TRANSPARENT);
  ok &= e220_.setModuleAddr(LORA_ADDR);
  ok &= e220_.setChannel(channel_);
  ok &= e220_.setRSSIEnable(true);
  ok &= e220_.setMode(E220::Mode::NORMAL);
  lora_serial_.flush();
  lora_serial_.begin(115200);

  delay(100);

  if (ok) {
    LOG("LoRa setup complete.");
  } else {
    LOG("LoRa setup error.");
  }
  xSemaphoreTake(tx_mutex_, portMAX_DELAY);
  ready_ = ok;
  last_activity_ms_ = last_telemetry_ms_ = millis();
  xSemaphoreGive(tx_mutex_);
}

bool LoRa::canSendTelemetry() {
  if (!tx_mutex_ || xSemaphoreTake(tx_mutex_, 0) != pdTRUE) return false;
  const bool idle = ready_ && telemetry_ready_ && !telemetry_packet_ &&
                    !ack_queue_.count && !normal_queue_.count && !awaiting_ack_;
  xSemaphoreGive(tx_mutex_);
  return idle;
}

bool LoRa::queuePacket(const wcpp::Packet& packet, TxPriority priority) {
  // Leave room for the wire checksum and the receiver's RSSI entry.
  if (!packet || packet.size() < packet.header_size() || packet.size() > max_packet_size ||
      !tx_mutex_ || xSemaphoreTake(tx_mutex_, 0) != pdTRUE) return false;
  TxQueue& queue = priority == TxPriority::Ack ? ack_queue_ : normal_queue_;
  const bool telemetry = priority == TxPriority::Telemetry;
  if (!ready_ || (telemetry ? (!telemetry_ready_ || telemetry_packet_ ||
      ack_queue_.count || normal_queue_.count || awaiting_ack_) :
      queue.count == tx_queue_size)) {
    xSemaphoreGive(tx_mutex_);
    return false;
  }
  // getPacket() can refer into a command envelope: retain an independent copy.
  wcpp::Packet owned = kernel::kernel_.allocPacket(packet.size());
  if (!owned || !owned.copy(packet)) {
    xSemaphoreGive(tx_mutex_);
    return false;
  }
  if (telemetry) {
    telemetry_packet_ = owned;
  } else {
    queue.packets[(queue.head + queue.count) % tx_queue_size] = owned;
    ++queue.count;
    if (priority == TxPriority::Ack && awaiting_ack_) --awaiting_ack_;
  }
  telemetry_ready_ = false;
  xSemaphoreGive(tx_mutex_);
  return true;
}

void LoRa::loop() {
  if (!tx_mutex_) return;
  // Drain RX before considering any TX. Partial UART frames also inhibit TX.
  uint8_t data[255];
  const unsigned len = e220_.receive(data, sizeof(data));
  if (len > 0) {
    xSemaphoreTake(tx_mutex_, portMAX_DELAY);
    last_activity_ms_ = millis();
    telemetry_ready_ = false;
    xSemaphoreGive(tx_mutex_);

    // Validate the outer length/checksum before decodePacket() reads the header.
    const unsigned size = len - 1;
    if (size < 4 || size > max_packet_size || data[0] != size ||
        (data[3] != 0 && size < 7) ||
        wcpp::Packet::checksum(data, size) != data[size]) {
      LOG("LoRa receive error: invalid length/checksum");
    } else {
      wcpp::Packet packet = newPacket(size + 10);
      if (packet) {
        packet.copy(wcpp::Packet::decode(data));
        packet.append("Ss").setInt(e220_.getRSSI());
        if (tracker_scheduling_ && packet.isCommand() &&
            (packet.packet_id() == 't' || packet.packet_id() == 'c')) {
          // Close the RX -> Main -> ACK queue scheduling gap.
          xSemaphoreTake(tx_mutex_, portMAX_DELAY);
          ++awaiting_ack_;
          xSemaphoreGive(tx_mutex_);
        }
        // LoRa has no catch-all listener: this is the sole RX publication.
        sendPacket(packet);
      }
    }
  }
  serviceTx();
}

void LoRa::serviceTx() {
  xSemaphoreTake(tx_mutex_, portMAX_DELAY);
  const uint32_t now = millis();
  const bool hardware_busy = e220_.isBusy() || lora_serial_.available() > 0;
  if (hardware_busy) last_activity_ms_ = now;
  if (tx_active_ && uint32_t(now - tx_started_ms_) >= tx_hold_ms_ && !hardware_busy) {
    tx_active_ = false;
    last_activity_ms_ = now;
  }
  // Preserve the proven 100 ms turnaround, without blocking the RX task.
  const bool idle = ready_ && !hardware_busy && !tx_active_ &&
                    uint32_t(now - last_activity_ms_) >= 100;
  telemetry_ready_ = idle && !awaiting_ack_ && !ack_queue_.count &&
                     !normal_queue_.count && !telemetry_packet_ &&
                     uint32_t(now - last_telemetry_ms_) >= 2000;
  if (!idle) {
    const bool report = (ack_queue_.count || normal_queue_.count) &&
                        uint32_t(now - last_busy_log_ms_) >= 2000;
    if (report) last_busy_log_ms_ = now;
    xSemaphoreGive(tx_mutex_);
    if (report) LOG("LoRa busy: TX retained for retry");
    return;
  }
  TxQueue* queue = ack_queue_.count ? &ack_queue_ :
                   (!awaiting_ack_ && normal_queue_.count ? &normal_queue_ : nullptr);
  const bool telemetry = !queue && telemetry_packet_ && !awaiting_ack_ &&
                         uint32_t(now - last_telemetry_ms_) >= 2000;
  if (!queue && !telemetry) {
    xSemaphoreGive(tx_mutex_);
    return;
  }
  const wcpp::Packet packet = queue ? queue->packets[queue->head] : telemetry_packet_;
  const unsigned size = packet.size();
  uint8_t data[255];
  memcpy(data, packet.encode(), size);
  data[size] = packet.checksum();
  // Check again at the write boundary; a failed admission leaves the queue intact.
  if (lora_serial_.available() || !e220_.sendTransparent(data, size + 1)) {
    telemetry_ready_ = false;
    last_activity_ms_ = now;
    xSemaphoreGive(tx_mutex_);
    return;
  }
  lora_serial_.flush(); // UART completion only, not RF completion.
  tx_active_ = true;
  tx_started_ms_ = millis();
  // Conservative SF9/BW125 guard incl. framing/module overhead. AUX high alone
  // does not prove RF TX has finished. Recalibrate if radio settings change.
  tx_hold_ms_ = 200 + (size + 16) * 10;
  telemetry_ready_ = false;
  if (queue) {
    queue->packets[queue->head] = wcpp::Packet::null();
    queue->head = (queue->head + 1) % tx_queue_size;
    --queue->count;
  } else {
    telemetry_packet_ = wcpp::Packet::null();
    last_telemetry_ms_ = tx_started_ms_;
  }
  xSemaphoreGive(tx_mutex_);
  if (telemetry && tracker_scheduling_) {
    LOG("[Tracker] Telemetry sent (UART accepted, Seq: %u)", packet.sequence());
  }
}

void LoRa::onCommand(const wcpp::Packet& packet) {
  // Keep the existing 's' + Pa command interface for other callers.
  // Never execute commands received over RF as local radio send envelopes.
  if (packet.packet_id() != send_command_id || packet.find("Ss")) return;
  const auto payload = packet.find("Pa");
  if (!payload || !(*payload).isPacket()) return;
  const wcpp::Packet inner = (*payload).getPacket();
  if (!inner) return;
  const TxPriority priority = inner.isTelemetry() && inner.packet_id() == 'a'
      ? TxPriority::Ack : TxPriority::Normal;
  if (!queuePacket(inner, priority)) LOG("LoRa TX queue full/not ready: command rejected");
}

}
