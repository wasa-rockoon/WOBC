#pragma once
#if defined(ARDUINO_ARCH_ESP32)
#include <library/wobc.h>
#include "e220.h"

namespace component {
// Ground: command -> uplink, downlink -> bus.
// Flight: uplink -> bus, 'M' telemetry -> downlink.
// Received packets carry Ss (RSSI) and are never retransmitted.
class DualLoRa : public process::Component {
public:
  static constexpr uint8_t component_id = 0x10;
  struct RadioConfig {
    unsigned uart_number;
    pin_t aux, m0, m1, uart_rx, uart_tx, rf_switch_1, rf_switch_2;
    uint8_t channel;
  };
  DualLoRa(const RadioConfig& uplink, const RadioConfig& downlink, bool ground);

protected:
  void setup() override;

  void loop() override;

private:
  enum class RadioSetup : uint8_t {
    OK,
    RX_BUFFER,
    BEGIN,
    CONFIG_MODE,
    CONFIG_READ,
    UART_BAUD,
    DATA_RATE,
    ENV_RSSI,
    SEND_MODE,
    ADDRESS,
    CHANNEL,
    PACKET_RSSI,
    NORMAL_MODE,
  };

  static const char* setupName(RadioSetup setup);

  RadioSetup configureRadio(unsigned uart_number, HardwareSerial& serial,
                            E220& radio, pin_t aux,
                            pin_t m0, pin_t m1, pin_t rx, pin_t tx,
                            pin_t rf_switch_1, pin_t rf_switch_2,
                            uint8_t channel);

  void logResponse(const E220& radio, unsigned baud);

  void receiveOne(E220& radio, HardwareSerial& serial, bool uplink_rx);

  void serviceTx(E220& radio, bool uplink_tx);

  static constexpr unsigned long lora_baud = 9600;
  static constexpr unsigned rssi_entry_size = 4;
  const RadioConfig uplink_, downlink_;
  const bool is_ground;
  const char* const role_name;
  HardwareSerial lora1_serial_;
  HardwareSerial lora2_serial_;
  E220 lora1_;
  E220 lora2_;
  kernel::Listener tx_listener_;
  wcpp::Packet pending_tx_ = wcpp::Packet::null();
  RadioSetup lora1_setup_ = RadioSetup::BEGIN;
  RadioSetup lora2_setup_ = RadioSetup::BEGIN;
  bool lora1_ready_ = false;
  bool lora2_ready_ = false;
};

}  // namespace component
#endif
