#include <library/wobc.h>
#include <Wire.h>
#include "src/BME280I2C.h"
#include <cmath>

namespace component {

class Pressure: public process::Component {
public:
  static const uint8_t component_id = 0x25; // TBD
  static const uint8_t telemetry_id = 'E'; // TBD
  static const int max_index = 32;  // p と coe の最大インデックス

  Pressure(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz = 1);

protected:
  TwoWire& wire_;
  BME280I2C bme;
  uint8_t unit_id_;

  struct PressureData {
    double pressure;
    double altitude;
  } p[max_index + 1];

  struct Coefficients {
    double a;
    double b;
    double c;
  } coe[max_index + 1];

  void setup() override;

  void initialize_pressure_data();
  void initialize_coefficients();
  double height(int pressure);

  class SampleTimer: public process::Timer {
  public:
    SampleTimer(Pressure& pressure_ref, BME280I2C& bme_ref, uint8_t unit_id_ref, unsigned interval_ms);

  protected:
    void callback() override;

  private:
    BME280I2C& bme_;
    Pressure& pressure_;
    uint8_t unit_id_;
  } sample_timer_;
};

}
