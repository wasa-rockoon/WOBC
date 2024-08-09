#include "indicator.h"

namespace core {

Indicator::Indicator(pin_t pin, bool invert)
  : Task("Indicator", WOBC_INDICATOR_STACK_SIZE, WOBC_INDICATOR_PRIORITY),
   pin_(pin), invert_(invert) {

}

void Indicator::begin() {
  startProcess(nullptr);
}

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

void Indicator::setup() {
  pinMode(pin_, OUTPUT);
}

void Indicator::loop() {
  digitalWrite(pin_, invert_ ? !get() : get());
  delay(1);
}

};