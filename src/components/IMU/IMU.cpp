#include "IMU.h"

namespace component {

IMU9::IMU9(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz)
  : process::Component("IMU", component_id),
    wire_(wire),
    IMU_(&::IMU),
    unit_id_(unit_id),
    imu_data_timer_(*this, IMU_, unit_id, (sample_freq_hz == 0 ? 5 : (1000 / sample_freq_hz))),
    servo_control_timer_(*this, unit_id) {
}

void IMU9::setup() {
  if (!IMU_->begin()) {
    error("IMU", "Failed to initialize IMU");
  }

  start(imu_data_timer_);      // 200Hz
}

IMU9::IMUDataTimer::IMUDataTimer(IMU9& IMU9_ref, BMI2_BMM1_Class* IMU_ref, uint8_t unit_id_ref, unsigned interval_ms)
  : process::Timer("IMUData", interval_ms),
    IMU9_(IMU9_ref), IMU_(IMU_ref), unit_id_(unit_id_ref) {
}

void IMU9::IMUDataTimer::callback() {
  float Ax = 0, Ay = 0, Az = 0;
  float Gx = 0, Gy = 0, Gz = 0;
  float Mx = 0, My = 0, Mz = 0;

  if (IMU_->accelerationAvailable()) {
    IMU_->readAcceleration(Ax, Ay, Az);
  }
  if (IMU_->gyroscopeAvailable()) {
    IMU_->readGyroscope(Gx, Gy, Gz);
  }
  if (IMU_->magneticFieldAvailable()) {
    IMU_->readMagneticField(Mx, My, Mz);
  }

  uint32_t timestamp_ms = millis();

  // シリアルに9軸の生データを出力
  Serial.printf("[IMU] t=%u Ax=%.3f Ay=%.3f Az=%.3f Gx=%.3f Gy=%.3f Gz=%.3f Mx=%.3f My=%.3f Mz=%.3f\n",
    timestamp_ms, Ax, Ay, Az, Gx, Gy, Gz, Mx, My, Mz);
}

IMU9::ServoControlTimer::ServoControlTimer(IMU9& IMU9_ref, uint8_t unit_id_ref)
  : process::Timer("ServoControl", 50),
    IMU9_(IMU9_ref), unit_id_(unit_id_ref) {
}

void IMU9::ServoControlTimer::callback() {
  // サーボ制御は不要
}















}  // namespace component
