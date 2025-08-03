#include <library/wobc.h>
#include "e220.h"

namespace component {

class LoRa: public process::Component {
public:
  static const uint8_t component_id_base = 0x10; // TBD
  static const uint8_t send_command_id = 's'; // TBD

  E220 e220_;

  LoRa(pin_t aux, pin_t m0, pin_t m1, pin_t tx, pin_t rx, uint8_t channel, unsigned number = 0);
  LoRa(pin_t aux, pin_t m0, pin_t m1, pin_t antenna_A, pin_t antenna_B, pin_t tx, pin_t rx, uint8_t channel, unsigned number = 0);

protected:
  HardwareSerial lora_serial_;
  bool antenna_switch_;
  pin_t antenna_A_;
  pin_t antenna_B_;
  pin_t tx_pin_;
  pin_t rx_pin_;
  pin_t aux_pin_;
  pin_t m0_pin_;
  pin_t m1_pin_;
  uint8_t channel_;

  kernel::Listener all_packets_; // リスナーをクラスメンバーとして定義

  void setup() override;
  void loop() override;
  void onCommand(const wcpp::Packet& packet) override;
};

}
