#if defined(ARDUINO_ARCH_ESP32)

#include "uplink.h"

#include <esp32-hal-uart.h>

namespace component {

Uplink::Uplink(const RadioConfig& config, Role role, uint8_t flight_unit_id)
    : process::Component("Uplink", component_id),
      config_(config),
      role_(role),
      flight_unit_id_(flight_unit_id),
      serial_(config_.uart_number),
      radio_(serial_, config_.aux, config_.m0, config_.m1) {
  priority_ = 1;
}

void Uplink::setup() {
  if (role_ == Role::Ground) {
    // Only remote flight-bound commands may occupy the uplink RF channel.
    tx_listener_.command().unit_dest(flight_unit_id_);
  } else {
    tx_listener_.telemetry().packet('a');
  }
  listen(tx_listener_, 16, false);

  radio_setup_ = configureRadio();
  ready_ = radio_setup_ == RadioSetup::OK;
  last_activity_ms_ = millis();
  LOG("ROLE=%s LoRa1=UPLINK_BIDIRECTIONAL(ch%u,UART%u) bring-up=%s",
      role_ == Role::Ground ? "GROUND" : "FLIGHT", config_.channel,
      config_.uart_number, setupName(radio_setup_));
  if (!ready_) error("upI", "Uplink LoRa bring-up failed: %s", setupName(radio_setup_));
}

void Uplink::loop() {
  if (!ready_) return;
  // Match the proven GOLIDEN/Tracker order: always drain RX before admitting TX.
  receiveOne();
  collectTxPackets();
  serviceTx();
}

const char* Uplink::setupName(RadioSetup setup) {
  switch (setup) {
  case RadioSetup::OK: return "OK";
  case RadioSetup::RX_BUFFER: return "RX_BUFFER";
  case RadioSetup::BEGIN: return "E220_BEGIN";
  case RadioSetup::CONFIG_MODE: return "CONFIG_MODE";
  case RadioSetup::CONFIG_READ: return "CONFIG_READ";
  case RadioSetup::UART_BAUD: return "UART_BAUD";
  case RadioSetup::DATA_RATE: return "DATA_RATE";
  case RadioSetup::ENV_RSSI: return "ENV_RSSI";
  case RadioSetup::SEND_MODE: return "SEND_MODE";
  case RadioSetup::ADDRESS: return "ADDRESS";
  case RadioSetup::CHANNEL: return "CHANNEL";
  case RadioSetup::PACKET_RSSI: return "PACKET_RSSI";
  case RadioSetup::NORMAL_MODE: return "NORMAL_MODE";
  }
  return "UNKNOWN";
}

Uplink::RadioSetup Uplink::configureRadio() {
  pinMode(config_.rf_switch_1, OUTPUT);
  pinMode(config_.rf_switch_2, OUTPUT);
  digitalWrite(config_.rf_switch_1, HIGH);
  digitalWrite(config_.rf_switch_2, LOW);

  if (serial_.setRxBufferSize(512) < 512) return RadioSetup::RX_BUFFER;
  serial_.begin(9600, SERIAL_8N1, config_.uart_rx, config_.uart_tx);
  LOG("UART%u ready: requested rx=%u tx=%u attached rx_gpio=%d tx_gpio=%d",
      config_.uart_number, config_.uart_rx, config_.uart_tx,
      uart_get_RxPin(config_.uart_number), uart_get_TxPin(config_.uart_number));
  if (!radio_.begin()) return RadioSetup::BEGIN;
  vTaskDelay(pdMS_TO_TICKS(1000));
  if (!radio_.setMode(E220::Mode::CONFIG_DS)) return RadioSetup::CONFIG_MODE;

  RadioSetup result = RadioSetup::OK;
  uint8_t parameters[8] = {};
  constexpr unsigned probe_bauds[] = {
      9600, 115200, 1200, 2400, 4800, 19200, 38400, 57600,
  };
  bool config_read_ok = false;
  for (const unsigned baud : probe_bauds) {
    serial_.updateBaudRate(baud);
    delay(20);
    while (serial_.available() > 0) serial_.read();
    config_read_ok = radio_.readRegister(E220::ADDR::ADDH, parameters,
                                         sizeof(parameters));
    if (config_read_ok) break;
    logResponse(baud);
  }

  if (!config_read_ok) {
    result = RadioSetup::CONFIG_READ;
  } else if (!radio_.setSerialBaudRate(lora_baud)) {
    result = RadioSetup::UART_BAUD;
  } else {
    serial_.flush();
    serial_.updateBaudRate(lora_baud);
    if (!radio_.setDataRate(E220::SF::SF9, E220::BW::BW125kHz)) result = RadioSetup::DATA_RATE;
    else if (!radio_.setEnvRSSIEnable(true)) result = RadioSetup::ENV_RSSI;
    else if (!radio_.setSendMode(E220::SendMode::TRANSPARENT)) result = RadioSetup::SEND_MODE;
    else if (!radio_.setModuleAddr(E220::BROADCAST)) result = RadioSetup::ADDRESS;
    else if (!radio_.setChannel(config_.channel)) result = RadioSetup::CHANNEL;
    else if (!radio_.setRSSIEnable(true)) result = RadioSetup::PACKET_RSSI;
  }

  const bool normal_mode = radio_.setMode(E220::Mode::NORMAL);
  serial_.flush();
  serial_.updateBaudRate(lora_baud);
  vTaskDelay(pdMS_TO_TICKS(100));
  if (result != RadioSetup::OK) return result;
  return normal_mode ? RadioSetup::OK : RadioSetup::NORMAL_MODE;
}

void Uplink::logResponse(unsigned baud) {
  LOG("E220 response baud=%u length=%u first=%02X %02X %02X %02X",
      baud, radio_.lastResponseLength(), radio_.lastResponseByte(0),
      radio_.lastResponseByte(1), radio_.lastResponseByte(2),
      radio_.lastResponseByte(3));
}

void Uplink::receiveOne() {
  uint8_t data[255];
  const unsigned len = radio_.receive(data, sizeof(data));
  if (len == 0) return;
  last_activity_ms_ = millis();

  if (len < 5 || data[0] != len - 1) {
    error("upSZ", "Uplink frame length mismatch");
    return;
  }
  const unsigned packet_size = len - 1;
  const unsigned header_size = data[3] == wcpp::unit_id_local ? 4 : 7;
  if (packet_size < header_size || packet_size + rssi_entry_size > wcpp::size_max ||
      wcpp::Packet::checksum(data, packet_size) != data[packet_size]) {
    error("upCS", "Uplink packet length/checksum invalid");
    return;
  }

  const wcpp::Packet received = wcpp::Packet::decode(data);
  const bool accepted = role_ == Role::Ground
      ? received.isTelemetry() && received.packet_id() == 'a'
      : received.isCommand();
  if (!accepted) {
    LOG("[UPLINK RX DROP] role=%s type=%s packet_id=0x%02X",
        role_ == Role::Ground ? "GROUND" : "FLIGHT",
        received.isCommand() ? "command" : "telemetry", received.packet_id());
    return;
  }

  if (role_ == Role::Ground && matchesInflightCommand(received)) {
    LOG("[UPLINK ACK MATCH] command=0x%02X sequence=%u attempts=%u",
        inflight_command_.packet_id(), inflight_command_.sequence(),
        transmit_attempts_);
    awaiting_ack_ = false;
    inflight_command_ = wcpp::Packet::null();
    transmit_attempts_ = 0;
  }

  wcpp::Packet packet = newPacket(packet_size + rssi_entry_size);
  if (!packet || !packet.copy(received) ||
      !packet.append("Ss").setInt(radio_.getRSSI())) {
    error("upSS", "Uplink RSSI append failed");
    return;
  }
  LOG("[UPLINK RX] role=%s type=%s packet_id=0x%02X sequence=%u",
      role_ == Role::Ground ? "GROUND" : "FLIGHT",
      packet.isCommand() ? "command" : "telemetry", packet.packet_id(),
      packet.sequence());
  sendPacket(packet);
}

bool Uplink::enqueue(const wcpp::Packet& packet) {
  if (!packet || tx_queue_.count == tx_queue_size) return false;
  wcpp::Packet owned = kernel::kernel_.allocPacket(packet.size());
  if (!owned || !owned.copy(packet)) return false;
  tx_queue_.packets[(tx_queue_.head + tx_queue_.count) % tx_queue_size] = owned;
  ++tx_queue_.count;
  return true;
}

wcpp::Packet Uplink::dequeue() {
  if (!tx_queue_.count) return wcpp::Packet::null();
  wcpp::Packet packet = tx_queue_.packets[tx_queue_.head];
  tx_queue_.packets[tx_queue_.head] = wcpp::Packet::null();
  tx_queue_.head = (tx_queue_.head + 1) % tx_queue_size;
  --tx_queue_.count;
  return packet;
}

void Uplink::collectTxPackets() {
  while (tx_listener_ && tx_queue_.count < tx_queue_size) {
    // Peek before admission: allocation failure must leave the command in the
    // listener for a later retry instead of silently losing it.
    const wcpp::Packet candidate = tx_listener_.peek();
    if (!candidate) return;
    // Ss identifies a packet that has already crossed RF. Sending it again
    // would create a Ground/Flight retransmission loop.
    // GOLIDEN also excludes locally generated Im control envelopes.
    if (candidate.find("Ss") ||
        (role_ == Role::Ground &&
         (!candidate.isRemote() || candidate.dest_unit_id() != flight_unit_id_ ||
          candidate.find("Im") || candidate.size() > 253))) {
      tx_listener_.pop();
      continue;
    }
    if (!enqueue(candidate)) {
      error("upQ", "Uplink TX queue allocation failed");
      return;
    }
    tx_listener_.pop();
  }
}

bool Uplink::matchesInflightCommand(const wcpp::Packet& ack) const {
  if (!awaiting_ack_ || !inflight_command_ || !ack.isRemote() ||
      ack.dest_unit_id() != kernel::unit_id() ||
      ack.sequence() != inflight_command_.sequence()) return false;
  const auto response_id = ack.find("Ri");
  const auto response_sequence = ack.find("Sq");
  return response_id && response_sequence && (*response_id).isInt() &&
         (*response_sequence).isInt() &&
         (*response_id).getInt() == inflight_command_.packet_id() &&
         (*response_sequence).getInt() == inflight_command_.sequence();
}

void Uplink::serviceTx() {
  const uint32_t now = millis();
  const bool hardware_busy = radio_.isBusy() || serial_.available() > 0;
  if (hardware_busy) last_activity_ms_ = now;
  if (tx_active_ && uint32_t(now - tx_started_ms_) >= tx_hold_ms_ && !hardware_busy) {
    tx_active_ = false;
    last_activity_ms_ = now;
  }

  if (role_ == Role::Ground && awaiting_ack_ &&
      int32_t(now - ack_deadline_ms_) >= 0) {
    awaiting_ack_ = false;
    if (transmit_attempts_ < max_transmit_attempts) {
      pending_tx_ = inflight_command_;
      LOG("[UPLINK ACK TIMEOUT] command=0x%02X sequence=%u retry=%u/%u",
          inflight_command_.packet_id(), inflight_command_.sequence(),
          transmit_attempts_ + 1, max_transmit_attempts);
    } else {
      error("upTO", "ACK timeout id=%02X seq=%u tries=%u",
            inflight_command_.packet_id(), inflight_command_.sequence(),
            transmit_attempts_);
      inflight_command_ = wcpp::Packet::null();
      transmit_attempts_ = 0;
    }
  }

  if (!pending_tx_) {
    if (role_ == Role::Ground) {
      // Stop-and-wait: do not start another command before the current ACK.
      if (!inflight_command_ && tx_queue_.count) {
        inflight_command_ = dequeue();
        pending_tx_ = inflight_command_;
      }
    } else if (tx_queue_.count) {
      pending_tx_ = dequeue();
    }
  }
  if (!pending_tx_ || hardware_busy || tx_active_ ||
      uint32_t(now - last_activity_ms_) < turnaround_guard_ms) return;

  const unsigned size = pending_tx_.size();
  if (size < pending_tx_.header_size() || size > 253) {
    error("upTX", "Uplink TX packet size invalid");
    pending_tx_ = wcpp::Packet::null();
    if (role_ == Role::Ground) {
      inflight_command_ = wcpp::Packet::null();
      transmit_attempts_ = 0;
    }
    return;
  }
  uint8_t data[255];
  memcpy(data, pending_tx_.encode(), size);
  data[size] = pending_tx_.checksum();
  if (!radio_.sendTransparent(data, size + 1)) return;

  serial_.flush();
  tx_active_ = true;
  tx_started_ms_ = millis();
  tx_hold_ms_ = 200 + (size + 16) * 10;
  if (role_ == Role::Ground) {
    ++transmit_attempts_;
    awaiting_ack_ = true;
    ack_deadline_ms_ = tx_started_ms_ + tx_hold_ms_ + ack_timeout_ms;
  }
  LOG("[UPLINK TX] role=%s type=%s packet_id=0x%02X sequence=%u",
      role_ == Role::Ground ? "GROUND" : "FLIGHT",
      pending_tx_.isCommand() ? "command" : "telemetry",
      pending_tx_.packet_id(), pending_tx_.sequence());
  pending_tx_ = wcpp::Packet::null();
}

}  // namespace component

#endif
