#include <library/wobc.h>
#include "e220.h"

namespace component {

class LoRa: public process::Component {
public:
  static const uint8_t component_id_base = 0x10; // TBD
  static const uint8_t send_command_id = 's'; // TBD

  LoRa(Stream& stream, pin_t aux, pin_t m0, pin_t m1, unsigned number = 0);
  LoRa(Stream& stream, pin_t aux, pin_t m0, pin_t m1, pin_t antenna_A, pin_t antenna_B, unsigned number = 0); // アンテナ切り替え有りver.

protected:
  E220 e220_;
  bool antenna_switch_;
  pin_t antenna_A_;
  pin_t antenna_B_;

  void setup() override;
  void loop() override;
  void onCommand(const wcpp::Packet& packet) override;
};


}
