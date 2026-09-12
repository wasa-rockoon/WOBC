#if defined(ARDUINO_ARCH_ESP32)
#include "dual_lora.h"
#include <esp32-hal-uart.h>

namespace component {
DualLoRa::DualLoRa(const RadioConfig& uplink, const RadioConfig& downlink, bool ground)
    : process::Component("DualLoRa", component_id),
      uplink_(uplink), downlink_(downlink), is_ground(ground),
      role_name(ground ? "GROUND" : "FLIGHT"),
      lora1_serial_(uplink_.uart_number),
      lora2_serial_(downlink_.uart_number),
      lora1_(lora1_serial_, uplink_.aux,
             uplink_.m0, uplink_.m1),
      lora2_(lora2_serial_, downlink_.aux,
             downlink_.m0, downlink_.m1) {
  priority_ = 1;
}

void DualLoRa::setup() {
  LOG("LoRa build=DUAL_UART uplink_UART=%u baud=%lu",
      uplink_.uart_number, lora_baud);
  // USB input without Ss is the only TX source. Ground accepts commands;
  // Flight accepts telemetry, keeping the two directions unambiguous.
  if (is_ground) tx_listener_.command();
  else tx_listener_.telemetry().packet('M');
  listen(tx_listener_, 16, false);

  lora1_setup_ = configureRadio(
    uplink_.uart_number,
    lora1_serial_, lora1_, uplink_.aux,
    uplink_.m0, uplink_.m1,
    uplink_.uart_rx,
      uplink_.uart_tx,
      uplink_.rf_switch_1,
      uplink_.rf_switch_2, uplink_.channel);
  lora2_setup_ = configureRadio(
    downlink_.uart_number,
    lora2_serial_, lora2_, downlink_.aux,
    downlink_.m0, downlink_.m1,
    downlink_.uart_rx,
      downlink_.uart_tx,
      downlink_.rf_switch_1,
      downlink_.rf_switch_2, downlink_.channel);
  lora1_ready_ = lora1_setup_ == RadioSetup::OK;
  lora2_ready_ = lora2_setup_ == RadioSetup::OK;

  LOG("ROLE=%s LoRa1=UPLINK(ch%u,UART%u) bring-up=%s", role_name,
      uplink_.channel, uplink_.uart_number, setupName(lora1_setup_));
  LOG("ROLE=%s LoRa2=DOWNLINK(ch%u,UART%u) bring-up=%s", role_name,
      downlink_.channel, downlink_.uart_number, setupName(lora2_setup_));
  if (!lora1_ready_) {
    error("lr1I", "LoRa1 bring-up failed: %s", setupName(lora1_setup_));
  }
  if (lora1_ready_) {
    LOG("UPLINK LoRa1 ready; uplink test is enabled");
  } else {
    LOG("UPLINK LoRa1 unavailable; uplink test is disabled");
  }
}

void DualLoRa::loop() {
  if (is_ground) {
    if (lora2_ready_) receiveOne(lora2_, lora2_serial_, false);
    if (lora1_ready_) serviceTx(lora1_, true);
  } else {
    if (lora1_ready_) receiveOne(lora1_, lora1_serial_, true);
    if (lora2_ready_) serviceTx(lora2_, false);
  }
}

const char* DualLoRa::setupName(RadioSetup setup) {
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

DualLoRa::RadioSetup DualLoRa::configureRadio(unsigned uart_number, HardwareSerial& serial,
                          E220& radio, pin_t aux,
                          pin_t m0, pin_t m1, pin_t rx, pin_t tx,
                          pin_t rf_switch_1, pin_t rf_switch_2,
                          uint8_t channel) {
  // RF path selection is separate from the E220 channel register below.
  pinMode(rf_switch_1, OUTPUT);
  pinMode(rf_switch_2, OUTPUT);
  digitalWrite(rf_switch_1, HIGH);
  digitalWrite(rf_switch_2, LOW);

  if (serial.setRxBufferSize(512) < 512) return RadioSetup::RX_BUFFER;
  serial.begin(9600, SERIAL_8N1, rx, tx);
  LOG("UART%u ready: requested rx=%u tx=%u attached rx_gpio=%d tx_gpio=%d",
      uart_number, rx, tx, uart_get_RxPin(uart_number),
      uart_get_TxPin(uart_number));
  if (!radio.begin()) {
    LOG("E220_BEGIN failed: uart_rx=%u uart_tx=%u aux=%d m0=%d m1=%d",
        rx, tx, digitalRead(aux), digitalRead(m0), digitalRead(m1));
    return RadioSetup::BEGIN;
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
  if (!radio.setMode(E220::Mode::CONFIG_DS)) return RadioSetup::CONFIG_MODE;
  RadioSetup result = RadioSetup::OK;
  // E220 keeps its configured UART baud across resets.  Probe every UART
  // speed accepted by the module before calling it a UART-path failure.
  // This is startup-only and does not alter the radio unless a valid reply
  // has already been received.
  uint8_t current_parameters[8] = {};
  constexpr unsigned probe_bauds[] = {
      9600, 115200, 1200, 2400, 4800, 19200, 38400, 57600,
  };
  bool config_read_ok = false;
  for (const unsigned baud : probe_bauds) {
    serial.updateBaudRate(baud);
    // A mode/baud transition can leave an E220 response byte in the UART RX
    // buffer. Discard that stale snapshot before issuing the next request so
    // readRegister() starts at its expected C1 header.
    delay(20);
    const int buffered = serial.available();
    uint8_t first[4] = {};
    unsigned drained = 0;
    const unsigned limit = buffered > 512 ? 512 : unsigned(buffered);
    while (drained < limit) {
      const int value = serial.read();
      if (value < 0) break;
      if (drained < sizeof(first)) first[drained] = uint8_t(value);
      ++drained;
    }
    LOG("PRE_RX baud=%u buffered=%d drained=%u first=%02X %02X %02X %02X",
        baud, buffered, drained, first[0], first[1], first[2], first[3]);
    config_read_ok = radio.readRegister(E220::ADDR::ADDH,
                                         current_parameters,
                                         sizeof(current_parameters));
    LOG("E220_CONFIG probe baud=%u result=%s", baud,
        config_read_ok ? "OK" : "NG");
    if (config_read_ok) break;
    logResponse(radio, baud);
  }
  if (!config_read_ok) {
    result = RadioSetup::CONFIG_READ;
  } else if (!radio.setSerialBaudRate(lora_baud)) {
    result = RadioSetup::UART_BAUD;
  } else {
    // The E220 applies the new rate after acknowledging the preceding
    // register write.  The ESP UART must switch before the next command.
    serial.flush();
    serial.updateBaudRate(lora_baud);
    if (!radio.setDataRate(E220::SF::SF9, E220::BW::BW125kHz)) result = RadioSetup::DATA_RATE;
    else if (!radio.setEnvRSSIEnable(true)) result = RadioSetup::ENV_RSSI;
    else if (!radio.setSendMode(E220::SendMode::TRANSPARENT)) result = RadioSetup::SEND_MODE;
    else if (!radio.setModuleAddr(E220::BROADCAST)) result = RadioSetup::ADDRESS;
    else if (!radio.setChannel(channel)) result = RadioSetup::CHANNEL;
    else if (!radio.setRSSIEnable(true)) result = RadioSetup::PACKET_RSSI;
  }
  const bool normal_mode = radio.setMode(E220::Mode::NORMAL);
  serial.flush();
  serial.updateBaudRate(lora_baud);
  vTaskDelay(pdMS_TO_TICKS(100));
  if (result != RadioSetup::OK) return result;
  return normal_mode ? RadioSetup::OK : RadioSetup::NORMAL_MODE;
}

void DualLoRa::logResponse(const E220& radio, unsigned baud) {
  LOG("E220 response baud=%u length=%u bytes=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
      baud, radio.lastResponseLength(), radio.lastResponseByte(0),
      radio.lastResponseByte(1), radio.lastResponseByte(2),
      radio.lastResponseByte(3), radio.lastResponseByte(4),
      radio.lastResponseByte(5), radio.lastResponseByte(6),
      radio.lastResponseByte(7), radio.lastResponseByte(8),
      radio.lastResponseByte(9), radio.lastResponseByte(10));
}

void DualLoRa::receiveOne(E220& radio, HardwareSerial& serial, bool uplink_rx) {
  // This is intentionally before E220 framing and WCPP validation.  It
  // distinguishes "nothing arrived at the UART" from a malformed frame.
  if (uplink_rx && serial.available() > 0) {
    LOG("[UPLINK RX UART] buffered_bytes=%d", serial.available());
  }
  uint8_t data[255];
  const unsigned len = radio.receive(data, sizeof(data));
  if (len == 0) return;
  if (uplink_rx) LOG("[UPLINK RX] bytes=%u", len);
  if (len < 5 || data[0] != len - 1) {
    if (uplink_rx) LOG("[UPLINK RX] frame length NG - packet dropped");
    error("lrSZ", "LoRa frame length mismatch");
    return;
  }
  const unsigned packet_size = len - 1;
  const unsigned header_size = data[3] == wcpp::unit_id_local ? 4 : 7;
  if (packet_size < header_size ||
      packet_size + rssi_entry_size > wcpp::size_max) {
    if (uplink_rx) LOG("[UPLINK RX] packet length NG - packet dropped");
    error("lrSZ", "LoRa packet length invalid");
    return;
  }
  const uint8_t crc_calculated = wcpp::Packet::checksum(data, packet_size);
  const uint8_t crc_received = data[packet_size];
  if (uplink_rx) {
    LOG("[UPLINK RX] crc_received=0x%02X crc_calculated=0x%02X CRC=%s",
        crc_received, crc_calculated,
        crc_calculated == crc_received ? "OK" : "NG");
  }
  if (crc_calculated != crc_received) {
    if (uplink_rx) LOG("[UPLINK RX] CRC NG - packet dropped");
    error("lrCS", "LoRa checksum mismatch");
    return;
  }

  const wcpp::Packet received = wcpp::Packet::decode(data);
  if (uplink_rx) {
    LOG("[UPLINK PACKET] type=%s packet_id=0x%02X component=0x%02X "
        "origin=0x%02X destination=0x%02X sequence=%u",
        received.isCommand() ? "command" : "telemetry",
        received.packet_id(), received.component_id(),
        received.origin_unit_id(), received.dest_unit_id(), received.sequence());
  }
  wcpp::Packet packet = newPacket(packet_size + rssi_entry_size);
  if (!packet || !packet.copy(received) ||
      !packet.append("Ss").setInt(radio.getRSSI())) {
    error("lrSS", "LoRa RSSI append failed");
    return;
  }
  // Publish to SerialBus and all normal listeners.  The TX listener has a
  // role-specific filter (GROUND=command, FLIGHT=M telemetry), so routing
  // RX directly to it would hide valid packets from USB.
  sendPacket(packet);
}

void DualLoRa::serviceTx(E220& radio, bool uplink_tx) {
  if (!pending_tx_ && tx_listener_) {
    wcpp::Packet candidate = tx_listener_.pop();
    if (!candidate.find("Ss")) {
      pending_tx_ = candidate;
    } else if (uplink_tx) {
      LOG("[UPLINK DROP] reason=received_RSSI_entry");
    }
  }
  if (!pending_tx_ || radio.isBusy()) return;
  const unsigned size = pending_tx_.size();
  if (size < pending_tx_.header_size() || size > 253) {
    error("lrTX", "LoRa TX packet size invalid");
    pending_tx_ = wcpp::Packet::null();
    return;
  }
  uint8_t data[255];
  memcpy(data, pending_tx_.encode(), size);
  data[size] = pending_tx_.checksum();
  if (uplink_tx) {
    LOG("[UPLINK TX] type=%s packet_id=0x%02X component=0x%02X "
        "origin=0x%02X destination=0x%02X sequence=%u payload_size=%u",
        pending_tx_.isCommand() ? "command" : "telemetry",
        pending_tx_.packet_id(), pending_tx_.component_id(),
        pending_tx_.origin_unit_id(), pending_tx_.dest_unit_id(),
        pending_tx_.sequence(), size - pending_tx_.header_size());
    LOG("[E220 TX] bytes=%u sendTransparent=called aux_busy_before=%s",
        size + 1, radio.isBusy() ? "yes" : "no");
  }
  const bool written = radio.sendTransparent(data, size + 1);
  if (uplink_tx) {
    LOG("[E220 TX] uart_write=%s aux_busy_after=%s",
        written ? "OK" : "NG", radio.isBusy() ? "yes" : "no");
  }
  if (written) {
    pending_tx_ = wcpp::Packet::null();
  }
}
}  // namespace component
#endif
