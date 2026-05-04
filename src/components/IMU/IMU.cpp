#include "IMU.h"

namespace component {

IMU9::IMU9(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz, int data_mode, int sensor_mode)
  : process::Component("IMU", component_id),
    wire_(wire),
    IMU_(&::IMU),  // IMU_を先に初期化
    unit_id_(unit_id),
    data_mode(data_mode),
    sensor_mode(sensor_mode),
    sample_timer_(*this, &::IMU, unit_id, sample_freq_hz),
    freq_(sample_freq_hz) {
    
}

void IMU9::setup() {
  if (sensor_mode == IMU_BMI_BMM) {
    if (!IMU_->begin()) {
    error("I", "Failed to initialize IMU!");
    }
  const uint8_t BMI270_ADDR = 0x68; 
  } else if (sensor_mode == IMU_ICM_MMC) {
    if (!MMC5603_.init()) {
      error("I", "Failed to initialize MMC5603!");
    }
  } else {
    error("I", "Invalid sensor mode!");
    return;
  }
  
  if (data_mode == IMU_DATA_WITH_MADGWICK_6) {
    gyro_offset_ = calibrate_gyro();
    filter.begin(freq_);
  }
  if (data_mode == IMU_DATA_WITH_KALMAN_6) {
    gyro_offset_ = calibrate_gyro();
    // Kalmanフィルタの初期化処理（未実装）
  }
  start(sample_timer_);
}

IMU9::SampleTimer::SampleTimer(IMU9& IMU9_ref, BoschSensorClass* IMU_ref, uint8_t unit_id_ref, unsigned sample_freq_hz)
  : process::Timer("IMU", sample_freq_hz > 0 ? 1000 / sample_freq_hz : 10),
    IMU9_(IMU9_ref), IMU_(IMU_ref), unit_id_(unit_id_ref) {
}

void IMU9::SampleTimer::callback() {
  uint32_t t0 = micros(); // ① デバッグ用タイマー　
  static float Ax = 0, Ay = 0, Az = 0;
  static float Gx = 0, Gy = 0, Gz = 0;
  static float Mx = 0, My = 0, Mz = 0;

  if (IMU9_.sensor_mode == IMU_BMI_BMM) {
    if (IMU_->accelerationAvailable()) { IMU_->readAcceleration(Ax, Ay, Az); }
    if (IMU_->gyroscopeAvailable()) { IMU_->readGyroscope(Gx, Gy, Gz); }
    if (IMU_->magneticFieldAvailable()) { IMU_->readMagneticField(Mx, My, Mz); }
  } else if (IMU9_.sensor_mode == IMU_ICM_MMC) {
    struct MMC5603::MagData mag_data = IMU9_.MMC5603_.read();
    Mx = mag_data.magX;
    My = mag_data.magY;
    Mz = mag_data.magZ;
  }
  uint32_t t1 = micros(); // ② I2C読み取り完了

  wcpp::Packet packet = newPacket(128);
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

  uint32_t t2 = micros(); // ③ パケット生成完了

  if (IMU9_.data_mode == IMU_DATA_WITH_MADGWICK_6) {
    float Gx_cal = Gx - IMU9_.gyro_offset_[0];
    float Gy_cal = Gy - IMU9_.gyro_offset_[1];
    float Gz_cal = Gz - IMU9_.gyro_offset_[2];
    IMU9_.filter.updateIMU(Gx_cal, Gy_cal, Gz_cal, Ax, Ay, Az);
    packet.append("Ro").setFloat32(IMU9_.filter.getRoll());
    packet.append("Pi").setFloat32(IMU9_.filter.getPitch());
    packet.append("Ya").setFloat32(IMU9_.filter.getYaw());
  }

  uint32_t t3 = micros(); // ④ Madgwick計算完了

  packet.append("Ts").setInt((int)millis());
  sendPacket(packet);

  uint32_t t4 = micros(); // ⑤ 全完了（送信完了）

  uint32_t total = t4 - t0;
  
  // もし合計が 5000us を超えたら、内訳を全部吐き出す！
  if (total > 0) { 
      Serial.printf("Total:%u | I2C:%u | Pkt1:%u | Madgwick:%u | Send:%u\n", 
                    total, (t1 - t0), (t2 - t1), (t3 - t2), (t4 - t3));
  }
}

std::array<float, 3> IMU9::calibrate_gyro() {
    const int num_samples = 200;
    std::array<float, 3> gyro_sum = {0.0f, 0.0f, 0.0f};

    for (int i = 0; i < num_samples; i++) {
        float gx, gy, gz;
        if (IMU_->gyroscopeAvailable()) {
            IMU_->readGyroscope(gx, gy, gz);
            gyro_sum[0] += gx;
            gyro_sum[1] += gy;
            gyro_sum[2] += gz;
        }
        delay(5); // サンプリング間隔
    }

    return {gyro_sum[0] / num_samples, gyro_sum[1] / num_samples, gyro_sum[2] / num_samples};
}
}
