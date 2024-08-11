#include "indicator.h"

namespace interface {


bool Indicator::get() {
  return millis() < on_until_ms_;
}
bool Indicator::toggle() {
  return set(!get());
}
bool Indicator::set(bool on) {
  if (on) on_until_ms_ = -1;
  else    on_until_ms_ = 0;
  digitalWrite(pin_, invert_ ? !get() : get());
  return on;
}
void Indicator::blink(unsigned ms) {
  on_until_ms_ = millis() + ms;
  digitalWrite(pin_, invert_ ? !get() : get());
}

void Indicator::begin() {
  pinMode(pin_, OUTPUT);
}

void Indicator::update() {
  digitalWrite(pin_, invert_ ? !get() : get());
  delay(1);
}

};