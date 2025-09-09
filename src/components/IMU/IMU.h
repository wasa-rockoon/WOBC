#ifndef ARDUINO
#define ARDUINO 100
#endif

#include <library/wobc.h>
#include <Arduino.h>
#include <Wire.h>
#include <Arduino_BMI270_BMM150.h>

// グローバル変数の外部宣言
extern BoschSensorClass IMU_BMI270_BMM150;

namespace component {

class IMU9: public process::Component {
public:
  static const uint8_t component_id = 0x30;
  static const uint8_t telemetry_id = 'I';

  IMU9(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz = 10);

protected:
  TwoWire& wire_;
  uint8_t unit_id_;
  BoschSensorClass* imu;

  void setup() override;

  void readallIMU();

  class SampleTimer: public process::Timer{
  public:
    SampleTimer(IMU9& IMU9_ref, BoschSensorClass& imu_ref, uint8_t& unit_id_ref, unsigned interval_ms);
  
  protected:
    void callback() override;
  
  private:
    IMU9& IMU9_;
    BoschSensorClass& imu_;
    uint8_t& unit_id_;
  } sample_timer_;
};
}