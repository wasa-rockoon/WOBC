#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include "e220.h"
#include <library/wobc.h>

namespace component {

// LoRa2 one-way telemetry link. ACK packet 'a' belongs to Uplink/LoRa1.
class Downlink : public process::Component {
public:
  static constexpr uint8_t component_id = 0x11;

  struct RadioConfig {
    unsigned uart_number;
    pin_t aux, m0, m1, uart_rx, uart_tx, rf_switch_1, rf_switch_2;
    uint8_t channel;
  };

  Downlink(const RadioConfig& config, bool ground);

protected:
  void setup() override;
  void loop() override;

private:
  bool configureRadio();
  void receiveOne();
  void serviceTx();

  static constexpr unsigned long lora_baud = 9600;
  static constexpr unsigned rssi_entry_size = 4;
  const RadioConfig config_;
  const bool is_ground_;
  HardwareSerial serial_;
  E220 radio_;
  kernel::Listener telemetry_listener_;
  wcpp::Packet pending_tx_ = wcpp::Packet::null();
  bool ready_ = false;
  bool tx_active_ = false;
  uint32_t tx_started_ms_ = 0;
  uint32_t tx_hold_ms_ = 0;
  uint32_t last_activity_ms_ = 0;

  static constexpr uint32_t turnaround_guard_ms = 100;
};

}  // namespace component

#endif
