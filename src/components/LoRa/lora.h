#pragma once

#include <library/wobc.h>
#include <semphr.h>
#include "e220.h"

namespace component {

class LoRa: public process::Component {
public:
  static const uint8_t component_id_base = 0x10; // TBD
  static const uint8_t send_command_id = 's'; // TBD
  static constexpr unsigned max_packet_size = 245; // Reserve receiver RSSI space.

  LoRa(pin_t aux, pin_t m0, pin_t m1, pin_t tx, pin_t rx, uint8_t channel, unsigned number = 0);
  LoRa(pin_t aux, pin_t m0, pin_t m1, pin_t antenna_A, pin_t antenna_B, pin_t tx, pin_t rx, uint8_t channel, unsigned number = 0);

  enum class TxPriority { Ack, Normal, Telemetry };
  // Nonblocking admission. On false the caller retains and retries the packet.
  // Only the LoRa task touches the UART; accepted packets are deep copied.
  bool queuePacket(const wcpp::Packet& packet, TxPriority priority);
  bool canSendTelemetry();
  void enableTrackerScheduling() { tracker_scheduling_ = true; } // Before begin().

protected:
  HardwareSerial lora_serial_;
  E220 e220_;
  bool antenna_switch_;
  pin_t antenna_A_;
  pin_t antenna_B_;
  pin_t tx_pin_;
  pin_t rx_pin_;
  pin_t aux_pin_;
  pin_t m0_pin_;
  pin_t m1_pin_;
  uint8_t channel_;

  static constexpr unsigned tx_queue_size = 8;
  struct TxQueue {
    wcpp::Packet packets[tx_queue_size] = {
      wcpp::Packet::null(), wcpp::Packet::null(), wcpp::Packet::null(), wcpp::Packet::null(),
      wcpp::Packet::null(), wcpp::Packet::null(), wcpp::Packet::null(), wcpp::Packet::null()
    };
    unsigned head = 0;
    unsigned count = 0;
  } ack_queue_, normal_queue_;
  wcpp::Packet telemetry_packet_ = wcpp::Packet::null();
  SemaphoreHandle_t tx_mutex_ = xSemaphoreCreateMutex();
  bool ready_ = false;
  bool telemetry_ready_ = false;
  bool tracker_scheduling_ = false;
  unsigned awaiting_ack_ = 0;
  bool tx_active_ = false;
  uint32_t tx_started_ms_ = 0;
  uint32_t tx_hold_ms_ = 0;
  uint32_t last_activity_ms_ = 0;
  uint32_t last_telemetry_ms_ = 0;
  uint32_t last_busy_log_ms_ = 0;

  void serviceTx();

  void setup() override;
  void loop() override;
  void onCommand(const wcpp::Packet& packet) override;
};

}
