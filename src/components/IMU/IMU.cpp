#include "IMU.h"

namespace component {

IMU9::IMU9(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz, int data_mode, int sensor_mode)
  : process::Component("IMU", component_id),
    wire_(wire),
    IMU_(&::IMU),
    ICM42688_(wire_, 0x68),
    MMC5603_(),
    unit_id_(unit_id),
    data_mode(data_mode),
    sensor_mode(sensor_mode),
    freq_(sample_freq_hz),
    sample_timer_(*this, &::IMU, unit_id, sample_freq_hz)
    {
    
}

void IMU9::setup() {
  if (sensor_mode == IMU_BMI_BMM) {
    if (!IMU_->begin()) {
    error("I", "Failed to initialize IMU!");
    }
  //const uint8_t BMI270_ADDR = 0x68; 
  } else if (sensor_mode == IMU_ICM_MMC) {
    if (!ICM42688_.begin()) {
      error("I", "Failed to initialize ICM42688!");
    }
    /*ICM42688_.setAccelFS(ICM42688::gpm16); // 加速度センサーのフルスケールレンジを±16gに設定
    ICM42688_.setGyroFS(ICM42688::dps2000); // ジャイロセンサーのフルスケールレンジを±2000dpsに設定
    ICM42688_.setAccelODR(ICM42688::odr200); // 加速度センサーを200Hzに設定
    ICM42688_.setGyroODR(ICM42688::odr200); // ジャイロセンサーを200Hzに設定
    ICM42688_.setFilters(true, false); // ジャイロセンサーのみにフィルタを適用
    ICM42688_.setGyroNotchFilter(166.7f, 166.7f, 166.7f, ICM42688::nfBW162Hz); // ジャイロセンサーのノッチフィルタを設定
    delay(100); // ICM42688の設定後に少し待機する
    */
    
    if (!MMC5603_.init()) {
      error("I", "Failed to initialize MMC5603!");
    }
  } else {
    error("I", "Invalid sensor mode!");
    return;
  }
  
  if (data_mode == IMU_DATA_WITH_MADGWICK_6) {
    if (sensor_mode == IMU_BMI_BMM) {
      gyro_offset_ = calibrate_gyro();
    } 
    filter.begin(freq_);
  }
  if (data_mode == IMU_DATA_WITH_KALMAN_6) {
    if (sensor_mode == IMU_BMI_BMM) {
      gyro_offset_ = calibrate_gyro();
    } 
  }
  delay(100); // センサーの初期化後に少し待機する
  can_counter = 0;
  start(sample_timer_);
}

IMU9::SampleTimer::SampleTimer(IMU9& IMU9_ref, BoschSensorClass* IMU_ref, uint8_t unit_id_ref, unsigned sample_freq_hz)
  : process::Timer("IMU", sample_freq_hz > 0 ? 1000 / sample_freq_hz : 10),
    IMU9_(IMU9_ref), IMU_(IMU_ref), unit_id_(unit_id_ref) {
}

void IMU9::SampleTimer::callback() {
  //uint32_t t0 = micros(); // ① デバッグ用タイマー　
  static float Ax = 0.0f, Ay = 0.0f, Az = 0.0f;
  static float Gx = 0.0f, Gy = 0.0f, Gz = 0.0f;
  static float Mx = 0.0f, My = 0.0f, Mz = 0.0f;
  static float Ro = 0.0f, Pi = 0.0f, Ya = 0.0f;
  uint8_t data_status = 0;
  // 0: ZOH（データ更新なし、またはエラー）
  // 1: ICMのみ更新
  // 2: MMCのみ更新
  // 3: ICMとMMCの両方が更新

  //ファイル切り替え時はI2Cを完全スキップ
  if (request_file_split.load()) {
    //static変数に残っている前回の正常な値（ZOH）をそのまま使う
  } 
  else {
    //I2C通信を行い、成功した時だけ値を更新する
    if (IMU9_.sensor_mode == IMU_BMI_BMM) {
      if (IMU_->accelerationAvailable()) { IMU_->readAcceleration(Ax, Ay, Az); }
      if (IMU_->gyroscopeAvailable()) { IMU_->readGyroscope(Gx, Gy, Gz); }
      if (IMU_->magneticFieldAvailable()) { IMU_->readMagneticField(Mx, My, Mz); }
    } 
    else if (IMU9_.sensor_mode == IMU_ICM_MMC) {
      
      // ICM42688の読み取り：成功(>0)した時のみstatic変数を上書きする
      if (IMU9_.ICM42688_.getAGT() > 0) {
        Ax = IMU9_.ICM42688_.accX();
        Ay = IMU9_.ICM42688_.accY();
        Az = IMU9_.ICM42688_.accZ();
        Gx = IMU9_.ICM42688_.gyrX();
        Gy = IMU9_.ICM42688_.gyrY();
        Gz = IMU9_.ICM42688_.gyrZ();
        data_status |= 0x01;
      }

      // MMC5603の読み取り：異常値(-32768)でなければ上書きする
      struct MMC5603::MagData mag_data = IMU9_.MMC5603_.read();
      if (mag_data.magX != -32768.0f) {
        Mx = mag_data.magX;
        My = mag_data.magY;
        Mz = mag_data.magZ;
        data_status |= 0x02;
      }
    }
  }
  //uint32_t t1 = micros(); // ② I2C読み取り完了

  if (data_status == 0x03) {
  if (IMU9_.data_mode == IMU_DATA_WITH_MADGWICK_6) {
    float Gx_cal = Gx - IMU9_.gyro_offset_[0];
    float Gy_cal = Gy - IMU9_.gyro_offset_[1];
    float Gz_cal = Gz - IMU9_.gyro_offset_[2];
    IMU9_.filter.updateIMU(Gx_cal, Gy_cal, Gz_cal, Ax, Ay, Az);
    Ro = IMU9_.filter.getRoll();
    Pi = IMU9_.filter.getPitch();
    Ya = IMU9_.filter.getYaw();
  }

  if (IMU9_.data_mode == IMU_DATA_WITH_KALMAN_6) {
    if (IMU9_.sensor_mode == IMU_BMI_BMM) {
      IMU9_.Gx_cal = Gx - IMU9_.gyro_offset_[0];
      IMU9_.Gy_cal = Gy - IMU9_.gyro_offset_[1];
      IMU9_.Gz_cal = Gz - IMU9_.gyro_offset_[2];
    } else if (IMU9_.sensor_mode == IMU_ICM_MMC) {
      IMU9_.Gx_cal = Gx;
      IMU9_.Gy_cal = Gy;
      IMU9_.Gz_cal = Gz;
    }
    KalmanFilter::angle angles = IMU9_.kalman_filter.update(IMU9_.Gx_cal, IMU9_.Gy_cal, IMU9_.Gz_cal, Ax, Ay, Az);
    Ro = angles.roll * 180.0f / PI;
    Pi = angles.pitch * 180.0f / PI;
    Ya = angles.yaw * 180.0f / PI;
  }
}
  //uint32_t t3 = micros(); // ④ Madgwick計算完了

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
  packet.append("Ro").setFloat32(Ro);
  packet.append("Pi").setFloat32(Pi);
  packet.append("Ya").setFloat32(Ya);
  packet.append("St").setInt(data_status);

  //uint32_t t2 = micros(); // ③ パケット生成完了
  
  packet.append("Ts").setInt((int)millis());
  sendPacket(packet);

  // Send compact packet for CAN bus
  // Contains only roll, pitch, yaw, and gyro data
  can_counter++;
  if (can_counter >= 2) { // Send every 2nd sample
    can_counter = 0;
    wcpp::Packet compact_packet = newPacket(48);
    compact_packet.telemetry('A', component_id(), unit_id_, 0xFF, 1234);
    compact_packet.append("Ro").setInt((int)(Ro * 100));      // Roll * 100 as int16
    compact_packet.append("Pi").setInt((int)(Pi * 100));      // Pitch * 100 as int16
    compact_packet.append("Ya").setInt((int)(Ya * 100));      // Yaw * 100 as int16
    compact_packet.append("Gx").setInt((int)(Gx * 100));      // Gyro X * 100 as int16
    compact_packet.append("Gy").setInt((int)(Gy * 100));      // Gyro Y * 100 as int16
    compact_packet.append("Gz").setInt((int)(Gz * 100));      // Gyro Z * 100 as int16
    compact_packet.append("Ax").setInt((int)(Ax * 100));      // Accel X * 100 as int16
    compact_packet.append("Ay").setInt((int)(Ay * 100));      // Accel Y * 100 as int16
    compact_packet.append("Az").setInt((int)(Az * 100));      // Accel Z * 100 as int16
    compact_packet.append("Ts").setInt((int)millis());
    sendPacket(compact_packet);
  }
  

  //uint32_t t4 = micros(); // ⑤ 全完了（送信完了）

  //uint32_t total = t4 - t0;
  /*uint32_t total = t4 - t0;
  if (total > 0) { 
      Serial.printf("Total:%u | I2C:%u | Pkt1:%u | Madgwick:%u | Send:%u\n", 
                    total, (t1 - t0), (t2 - t3), (t3 - t1), (t4 - t2));
  }*/
}

std::array<float, 3> IMU9::calibrate_gyro() {//ICM42688ではライブラリ内にキャリブレーション関数があるのでこれは使わなくてよい
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
