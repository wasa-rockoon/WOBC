#include "flight_pin.h"

namespace component {

FlightPin::FlightPin(uint8_t unit_id, pin_t pin, unsigned sample_freq_hz)
    : process::Component("FlightPin", component_id),
      sample_timer_(*this, sample_freq_hz ? 1000 / sample_freq_hz : 1000),
      unit_id_(unit_id), pin_(pin) {}

void FlightPin::setup() {
  pinMode(pin_, INPUT);
  state_ = digitalRead(pin_);
  state_changed_ms_ = millis();
  start(sample_timer_);
}

FlightPin::SampleTimer::SampleTimer(FlightPin& flight_pin,
                                    unsigned interval_ms)
    : process::Timer("FlightPinTimer", interval_ms), flight_pin_(flight_pin) {}

void FlightPin::SampleTimer::callback() {
  const uint32_t now = millis();
  const int state = digitalRead(flight_pin_.pin_);
  if (state != flight_pin_.state_) {
    flight_pin_.state_ = state;
    flight_pin_.state_changed_ms_ = now;
  }

  wcpp::Packet packet = newPacket(64);
  packet.telemetry(telemetry_id, component_id(), flight_pin_.unit_id_, 0xFF, 0);
  packet.append(stateEntryName()).setInt(state);
  packet.append("ET").setInt(now - flight_pin_.state_changed_ms_);
  packet.append("TS").setInt(now);
  sendPacket(packet);
}

}  // namespace component
