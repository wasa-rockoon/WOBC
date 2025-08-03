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
  WatchIndicator(pin_t pin, const T& target, bool invert = false)
    : Indicator(pin, invert), target_(target) {}

  void blink_on_change(unsigned blink_ms = 1) {
    mode_ = BLINK_ON_CHANGE;
    blink_ms_ = blink_ms;
    target_comparison_ = target_;
  }
  void on_while_equal_to(T value) {
    mode_ = ON_WHILE_EQUAL;
    target_comparison_ = value;
  }
  void on_while_not_equal_to(T value) {
    mode_ = ON_WHILE_NOT_EQUAL;
    target_comparison_ = value;
  }

  virtual void update() override {
    switch (mode_) {
    case BLINK_ON_CHANGE:
      if (target_ != target_comparison_) {
        blink(blink_ms_);
        target_comparison_ = target_;
      }
      break;
    case ON_WHILE_EQUAL:
      set(target_ == target_comparison_);
      break;
    case ON_WHILE_NOT_EQUAL:
      set(target_ != target_comparison_);
      break;
    default:
      break;
    }
    Indicator::update();
  }

protected:
  const T& target_;

  enum Mode {
    MANUAL,
    BLINK_ON_CHANGE,
    ON_WHILE_EQUAL,
    ON_WHILE_NOT_EQUAL,
  } mode_ = MANUAL;

  T target_comparison_;
  unsigned blink_ms_;
};

}