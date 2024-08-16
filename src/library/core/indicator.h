#pragma once

#include "library/process/task.h"

namespace core {

#ifndef WOBC_INDICATOR_STACK_SIZE
#define WOBC_INDICATOR_STACK_SIZE 1024
#endif
#ifndef WOBC_INDICATOR_PRIORITY
#define WOBC_INDICATOR_PRIORITY 0
#endif

class Indicator: public process::Task {
public:
  Indicator(pin_t pin, bool invert = false);

  void begin();

  bool toggle();
  bool get();
  bool set(bool on);
  void blink(unsigned ms);

protected:
  pin_t pin_;
  bool invert_;
  unsigned long on_until_ms_;

  virtual void setup() override;
  virtual void loop() override;
};

template<typename T>
class WatchIndicator: public Indicator {
public:
  WatchIndicator(pin_t pin, const T& target, unsigned blink_ms = 1, bool invert = false)
    : Indicator(pin, invert), target_(target), target_previous_(target), blink_ms_(blink_ms) {}

protected:
  virtual void loop() override {
    if (target_ != target_previous_) {
      blink(blink_ms_);
      target_previous_ = target_;
    }
    Indicator::loop();
  }

  const T& target_;
  T target_previous_;
  unsigned blink_ms_;
};

}