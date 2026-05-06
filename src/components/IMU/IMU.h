#ifndef ARDUINO
#define ARDUINO 100
#endif

#define IMU_BMI_BMM 0
#define IMU_ICM_MMC 1
#define IMU_DATA 0
#define IMU_DATA_WITH_MADGWICK_6 1
#define IMU_DATA_WITH_KALMAN_6 2

#include <library/wobc.h>
#include <Arduino.h>
#include <Wire.h>
#include <Arduino_BMI270_BMM150.h>
#include "src/MMC5603/MMC5603.h"
#include "src/ICM42688/ICM42688.h"
#include <MadgwickAHRS.h>
#include "src/Kalmanfilter/Kalmanfilter.h"
#include <array>

// グローバル変数の外部宣言
extern BoschSensorClass IMU;

namespace component {

class IMU9: public process::Component {
public:
  static const uint8_t component_id = 0x30;
  static const uint8_t telemetry_id = 'I';

  Madgwick filter;
  KalmanFilter kalman_filter;
  IMU9(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz = 100, int data_mode = IMU_DATA, int sensor_mode = IMU_BMI_BMM);

protected:
  TwoWire& wire_;
  BoschSensorClass* IMU_;
  ICM42688 ICM42688_;
  MMC5603 MMC5603_;
  uint8_t unit_id_;
  int data_mode = 0;
  int sensor_mode = 0;
  int freq_;
  std::array<float, 3> gyro_offset_ = {0.0f, 0.0f, 0.0f};
  std::array<float, 3> bias_magnetometer_ = {0.0f, 0.0f, 0.0f};
  float Gx_cal = 0, 
        Gy_cal = 0, 
        Gz_cal = 0;

  void setup() override;

  std::array<float, 3> calibrate_gyro();
  std::array<float, 3> calibrate_magnetometer();

  class SampleTimer: public process::Timer{
  public:
    SampleTimer(IMU9& IMU9_ref, BoschSensorClass* IMU_ref, uint8_t unit_id_ref, unsigned sample_freq_hz);
  
  protected:
    void callback() override;
  
  private:
    IMU9& IMU9_;
    BoschSensorClass* IMU_;
    uint8_t unit_id_;
  } sample_timer_;
};
}