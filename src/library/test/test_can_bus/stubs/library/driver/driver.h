#pragma once

#include "library/common.h"

namespace driver {

class CAN {
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
    virtual void onReceive(const Frame&) = 0;
    virtual void onError() = 0;
  };

  static CAN* active;
  std::vector<Frame> transmitted;
  explicit CAN(Receiver& receiver) : receiver_(receiver) { active = this; }
  bool begin(unsigned, pin_t, pin_t) { return true; }
  bool send(const Frame& frame) { transmitted.push_back(frame); return true; }
  void update() {}
  void inject(const Frame& frame) { receiver_.onReceive(frame); }

private:
  Receiver& receiver_;
};

} // namespace driver
