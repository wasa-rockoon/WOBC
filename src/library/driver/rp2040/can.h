#pragma once

#include "../can_if.h"
extern "C" {
  #include <can2040.h>
}

namespace driver {

class CAN: public CAN_IF {
public:
  CAN(Receiver& receiver): CAN_IF(receiver) {};

  bool begin(unsigned baudrate, pin_t rx, pin_t tx) override;
  bool send(const Frame& frame) override;

private:
  static void callback(struct can2040 *cd, uint32_t notify, struct can2040_msg *msg);
  
  struct can2040 cbus;

  static void PIOx_IRQHandler(void);
};

}