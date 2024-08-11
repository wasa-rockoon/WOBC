#pragma once

#include "library/process/task.h"

namespace interface {

class Indicator {
public:
  Indicator(pin_t pin, bool invert = false)
  : pin_(pin), invert_(invert) {}

  virtual void begin();
  virtual void update();

  bool toggle();
  bool get();
  bool set(bool on);
  void blink(unsigned ms);

protected:
  pin_t pin_;
  bool invert_;
  unsigned long on_until_ms_;
};

template<typename T>
class WatchIndicator: public Indicator {
public:
  WatchIndicator(pin_t pin, const T& target, unsigned blink_ms = 1, bool invert = false)
    : Indicator(pin, invert), target_(target), target_previous_(target), blink_ms_(blink_ms) {}

  virtual void update() override {
    if (target_ != target_previous_) {
      blink(blink_ms_);
      target_previous_ = target_;
    }
    Indicator::update();
  }

protected:

  const T& target_;
  T target_previous_;
  unsigned blink_ms_;
};

}