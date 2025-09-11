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
  if (!IMU_->begin()) {
    Serial.println("Failed to initialize IMU!");
  }
}

IMU9::SampleTimer::SampleTimer(IMU9& IMU9_ref, BoschSensorClass* IMU_ref, uint8_t unit_id_ref, unsigned sample_freq_hz)
  : process::Timer("IMU", 1000 / sample_freq_hz),
    IMU9_(IMU9_ref), IMU_(IMU_ref), unit_id_(unit_id_ref) {
}

void IMU9::SampleTimer::callback() {
  float Ax, Ay, Az, Gx, Gy, Gz, Mx, My, Mz;

  if (IMU_->accelerationAvailable()) {
    IMU_->readAcceleration(Ax, Ay, Az);
  }
  if (IMU_->gyroscopeAvailable()) {
    IMU_->readGyroscope(Gx, Gy, Gz);
  }
  if (IMU_->magneticFieldAvailable()) {
    IMU_->readMagneticField(Mx, My, Mz);
  }

  wcpp::Packet packet = newPacket(64);
  packet.telemetry(telemetry_id, component_id(), unit_id_, 0xFF, 1234);
  packet.append("Ax").setFloat32(Ax);
  packet.append("Ay").setFloat32(Ay);
  packet.append("Az").setFloat32(Az);
  packet.append("Gx").setFloat32(Gx);
  packet.append("Gy").setFloat32(Gy);
  packet.append("Gz").setFloat32(Gz);
  packet.append("Mx").setFloat32(Mx);
  packet.append("My").setFloat32(My);
  packet.append("Mz").setFloat32(Mz);

  //packet.append("Ts").setInt((int)millis());
  sendPacket(packet);
}
}
