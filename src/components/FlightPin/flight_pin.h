#pragma once

#include <library/wobc.h>

namespace component {

// Reports the physical flight-pin state. It does not control any output.
class FlightPin : public process::Component {
 public:
  static constexpr uint8_t component_id = 0x50;
  static constexpr uint8_t telemetry_id = 'F';
  static constexpr const char* stateEntryName() { return "Fp"; }

  FlightPin(uint8_t unit_id, pin_t pin, unsigned sample_freq_hz = 1);

 protected:
  void setup() override;

 private:
  class SampleTimer : public process::Timer {
   public:
    SampleTimer(FlightPin& flight_pin, unsigned interval_ms);

   protected:
    void callback() override;

   private:
    FlightPin& flight_pin_;
  } sample_timer_;

  const uint8_t unit_id_;
  const pin_t pin_;
  int state_ = LOW;
  uint32_t state_changed_ms_ = 0;
};

}  // namespace component
