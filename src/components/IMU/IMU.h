#ifndef ARDUINO
#define ARDUINO 100
#endif

#include <library/wobc.h>
#include <Arduino.h>
#include <Wire.h>
#include "BMI2_BMM1/BMI2_BMM1.h" //IMUセンサーのライブラリ。

// グローバル変数の外部宣言
extern BMI2_BMM1_Class IMU;

namespace component {

class IMU9: public process::Component {
public:
  static const uint8_t component_id = 0x30;
  static const uint8_t telemetry_id = 'I';

  IMU9(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz = 100);

protected:
  TwoWire& wire_;
  BMI2_BMM1_Class* IMU_;
  uint8_t unit_id_;

  void setup() override;

  class SampleTimer: public process::Timer{
  public:
    SampleTimer(IMU9& IMU9_ref, BMI2_BMM1_Class* IMU_ref, uint8_t unit_id_ref, unsigned interval_ms);
  
  protected:
    void callback() override;
  
  private:
    IMU9& IMU9_;
    BMI2_BMM1_Class* IMU_;
    uint8_t unit_id_;
  } sample_timer_;
};
}