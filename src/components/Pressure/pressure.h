#include <library/wobc.h>
#include <Wire.h>

namespace component {

class Pressure: public process::Component {
public:
  static const uint8_t component_id = 0x25; // TBD
  static const uint8_t telemetry_id = 'P'; // TBD

  Pressure(TwoWire& wire, unsigned sample_freq_hz = 10);

protected:
  TwoWire& wire_;

  void setup() override;

  class SampleTimer: public process::Timer {
  public:
    using process::Timer::Timer;

  protected:
    void callback() override;
  } sample_timer_;
};

}