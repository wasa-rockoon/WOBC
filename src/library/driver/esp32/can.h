#include "../can_if.h"
#include <esp32_can.h>

namespace driver {

class CAN: public CAN_IF {
public:
  CAN(Receiver& receiver): CAN_IF(receiver) {};

  bool begin(unsigned baudrate, pin_t rx, pin_t tx) override;
  bool send(const Frame& frame) override;

  void update() override;

private:
  static void callback(CAN_FRAME *frame);
};

}