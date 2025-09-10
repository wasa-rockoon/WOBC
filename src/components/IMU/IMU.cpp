#include "IMU.h"
#include <Arduino_BMI270_BMM150.h> // 明示的に再度インクルード

namespace component {

IMU9::IMU9(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz)
  : process::Component("IMU", component_id),
    wire_(wire),
    IMU_(&::IMU),  // IMU_を先に初期化
    unit_id_(unit_id),
    sample_timer_(*this, IMU_, unit_id, 1000 / sample_freq_hz) {
    
}

void IMU9::setup() {
  start(sample_timer_);
  Wire.begin();
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }
}

IMU9::SampleTimer::SampleTimer(IMU9& IMU9_ref, BoschSensorClass* IMU_ref, uint8_t unit_id_ref, unsigned sample_freq_hz)
  : process::Timer("IMU", 1000 / sample_freq_hz),
    IMU9_(IMU9_ref), IMU_(IMU_ref), unit_id_(unit_id_ref) {
}

}
