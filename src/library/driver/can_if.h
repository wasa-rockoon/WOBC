#pragma once

#include "library/common.h"

namespace driver {

class CAN_IF {
public:
  struct Frame {
    uint32_t id;
    uint8_t data[32];
    uint8_t length;
    bool extended;
    bool rtr;
  };

  class Receiver {
  public:
    virtual void onReceive(const Frame& frame) {};
    virtual void onError() {};
  };


  CAN_IF(Receiver& receiver): receiver_(receiver) {};

  virtual bool begin(unsigned baudrate, pin_t rx, pin_t tx) = 0;
  virtual bool send(const Frame& frame) = 0;

  virtual void update() {};
  
protected:
  Receiver& receiver_;
};

}