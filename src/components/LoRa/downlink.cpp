#if defined(ARDUINO_ARCH_ESP32)

#include "downlink.h"

namespace component {

Downlink::Downlink(const RadioConfig& config, bool ground)
    : process::Component("Downlink", component_id),
      config_(config),
      is_ground_(ground),
      serial_(config_.uart_number),
      radio_(serial_, config_.aux, config_.m0, config_.m1) {
  priority_ = 1;
}

void Downlink::setup() {
  if (!is_ground_) {
    telemetry_listener_.telemetry();
    listen(telemetry_listener_, 32, false);
  }
  ready_ = configureRadio();
  last_activity_ms_ = millis();
  LOG("ROLE=%s LoRa2=TELEMETRY_DOWNLINK(ch%u,UART%u) bring-up=%s",
      is_ground_ ? "GROUND" : "FLIGHT", config_.channel,
      config_.uart_number, ready_ ? "OK" : "NG");
  if (!ready_) error("dnI", "Downlink LoRa bring-up failed");
}

void Downlink::loop() {
  if (!ready_) return;
  if (is_ground_) receiveOne();
  else serviceTx();
}

bool Downlink::configureRadio() {
  pinMode(config_.rf_switch_1, OUTPUT);
  pinMode(config_.rf_switch_2, OUTPUT);
  digitalWrite(config_.rf_switch_1, HIGH);
  digitalWrite(config_.rf_switch_2, LOW);
  if (serial_.setRxBufferSize(512) < 512) return false;
  serial_.begin(9600, SERIAL_8N1, config_.uart_rx, config_.uart_tx);
  if (!radio_.begin()) return false;
  vTaskDelay(pdMS_TO_TICKS(1000));
  if (!radio_.setMode(E220::Mode::CONFIG_DS)) return false;

  uint8_t parameters[8] = {};
  constexpr unsigned probe_bauds[] = {
      9600, 115200, 1200, 2400, 4800, 19200, 38400, 57600,
  };
  bool config_read_ok = false;
  for (const unsigned baud : probe_bauds) {
    serial_.updateBaudRate(baud);
    delay(20);
    while (serial_.available() > 0) serial_.read();
    if (radio_.readRegister(E220::ADDR::ADDH, parameters, sizeof(parameters))) {
      config_read_ok = true;
      break;
    }
  }
  if (!config_read_ok || !radio_.setSerialBaudRate(lora_baud)) return false;
  serial_.flush();
  serial_.updateBaudRate(lora_baud);
  const bool ok = radio_.setDataRate(E220::SF::SF9, E220::BW::BW125kHz) &&
                  radio_.setEnvRSSIEnable(true) &&
                  radio_.setSendMode(E220::SendMode::TRANSPARENT) &&
                  radio_.setModuleAddr(E220::BROADCAST) &&
                  radio_.setChannel(config_.channel) &&
                  radio_.setRSSIEnable(true) &&
                  radio_.setMode(E220::Mode::NORMAL);
  serial_.flush();
  serial_.updateBaudRate(lora_baud);
  vTaskDelay(pdMS_TO_TICKS(100));
  return ok;
}

void Downlink::receiveOne() {
  uint8_t data[255];
  const unsigned len = radio_.receive(data, sizeof(data));
  if (len == 0) return;
  if (len < 5 || data[0] != len - 1) {
    error("dnSZ", "Downlink frame length mismatch");
    return;
  }
  const unsigned packet_size = len - 1;
  const unsigned header_size = data[3] == wcpp::unit_id_local ? 4 : 7;
  if (packet_size < header_size || packet_size + rssi_entry_size > wcpp::size_max ||
      wcpp::Packet::checksum(data, packet_size) != data[packet_size]) {
    error("dnCS", "Downlink packet length/checksum invalid");
    return;
  }
  const wcpp::Packet received = wcpp::Packet::decode(data);
  if (!received.isTelemetry() || received.packet_id() == 'a') return;
  wcpp::Packet packet = newPacket(packet_size + rssi_entry_size);
  if (!packet || !packet.copy(received) ||
      !packet.append("Ss").setInt(radio_.getRSSI())) {
    error("dnSS", "Downlink RSSI append failed");
    return;
  }
  sendPacket(packet);
}

void Downlink::serviceTx() {
  if (!pending_tx_) {
    while (telemetry_listener_) {
      const wcpp::Packet candidate = telemetry_listener_.pop();
      // Only forward flight-origin remote telemetry. ACK uses LoRa1 and Ss
      // identifies a packet that has already crossed an RF link.
      if (candidate.isRemote() &&
          candidate.origin_unit_id() == kernel::unit_id() &&
          candidate.packet_id() != 'a' && !candidate.find("Ss")) {
        pending_tx_ = candidate;
        break;
      }
    }
  }
  const uint32_t now = millis();
  const bool hardware_busy = radio_.isBusy() || serial_.available() > 0;
  if (hardware_busy) last_activity_ms_ = now;
  if (tx_active_ && uint32_t(now - tx_started_ms_) >= tx_hold_ms_ &&
      !hardware_busy) {
    tx_active_ = false;
    last_activity_ms_ = now;
  }
  if (!pending_tx_ || hardware_busy || tx_active_ ||
      uint32_t(now - last_activity_ms_) < turnaround_guard_ms) return;
  const unsigned size = pending_tx_.size();
  if (size < pending_tx_.header_size() || size > 253) {
    error("dnTX", "Downlink TX packet size invalid");
    pending_tx_ = wcpp::Packet::null();
    return;
  }
  uint8_t data[255];
  memcpy(data, pending_tx_.encode(), size);
  data[size] = pending_tx_.checksum();
  if (!radio_.sendTransparent(data, size + 1)) return;
  serial_.flush();  // UART completion is not RF completion.
  tx_active_ = true;
  tx_started_ms_ = millis();
  // Same conservative SF9/BW125 airtime guard as the bidirectional LoRa1.
  tx_hold_ms_ = 200 + (size + 16) * 10;
  LOG("[DOWNLINK TX] packet_id=0x%02X component=0x%02X sequence=%u",
      pending_tx_.packet_id(), pending_tx_.component_id(), pending_tx_.sequence());
  pending_tx_ = wcpp::Packet::null();
}

}  // namespace component

#endif
