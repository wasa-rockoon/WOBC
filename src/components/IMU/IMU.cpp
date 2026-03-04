#include "IMU.h"

namespace component {

IMU9::IMU9(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz)
  : process::Component("IMU", IMU9::component_id),
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

  // 最新データを更新
  IMU9_.latest_data_.Ax = Ax;
  IMU9_.latest_data_.Ay = Ay;
  IMU9_.latest_data_.Az = Az;
  IMU9_.latest_data_.Gx = Gx;
  IMU9_.latest_data_.Gy = Gy;
  IMU9_.latest_data_.Gz = Gz;
  IMU9_.latest_data_.Mx = Mx;
  IMU9_.latest_data_.My = My;
  IMU9_.latest_data_.Mz = Mz;
  IMU9_.latest_data_.timestamp_ms = timestamp_ms;

  // 相補フィルター実行
  IMU9_.updateQuaternion();

  // クォータニオン結果を保存
  IMU9_.latest_data_.qx = IMU9_.attitude_.q1;
  IMU9_.latest_data_.qy = IMU9_.attitude_.q2;
  IMU9_.latest_data_.qz = IMU9_.attitude_.q3;
  IMU9_.latest_data_.qw = IMU9_.attitude_.q0;

  wcpp::Packet packet = IMU9_.newPacket(80);
  packet.telemetry(IMU9::telemetry_id, IMU9::component_id, unit_id_, 0xFF, timestamp_ms);
  packet.append("Ts").setInt(timestamp_ms);
  packet.append("Ax").setFloat16(Ax);
  packet.append("Ay").setFloat16(Ay);
  packet.append("Az").setFloat16(Az);
  packet.append("Gx").setFloat16(Gx);
  packet.append("Gy").setFloat16(Gy);
  packet.append("Gz").setFloat16(Gz);
  packet.append("qx").setFloat16(IMU9_.latest_data_.qx);
  packet.append("qy").setFloat16(IMU9_.latest_data_.qy);
  packet.append("qz").setFloat16(IMU9_.latest_data_.qz);
  packet.append("qw").setFloat16(IMU9_.latest_data_.qw);
  //packet.append("Mx").setFloat32(Mx);
  //packet.append("My").setFloat32(My);
  //packet.append("Mz").setFloat32(Mz);

  // 200Hzのうち100回に1回だけLoRaへ流す（約2Hz）
  lora_decimation_counter_++;
  if (lora_decimation_counter_ >= 100) {
    lora_decimation_counter_ = 0;
    // Imなし: LoRaにも転送
  } else {
    // Imあり: SD保存のみ
    packet.append("Im").setNull();
  }

  // シリアルに9軸の生データを出力
  Serial.printf("[IMU] t=%u Ax=%.3f Ay=%.3f Az=%.3f Gx=%.3f Gy=%.3f Gz=%.3f qw=%.4f qx=%.4f qy=%.4f qz=%.4f\n",
    timestamp_ms, Ax, Ay, Az, Gx, Gy, Gz, IMU9_.latest_data_.qw, IMU9_.latest_data_.qx, IMU9_.latest_data_.qy, IMU9_.latest_data_.qz);
  IMU9_.sendPacket(packet);
}

IMU9::ServoControlTimer::ServoControlTimer(IMU9& IMU9_ref, uint8_t unit_id_ref)
  : process::Timer("ServoControl", 50),
    IMU9_(IMU9_ref), unit_id_(unit_id_ref) {
}

void IMU9::ServoControlTimer::callback() {
  // サーボ制御は不要
}

// 相補フィルター用メソッド実装

// クォータニオン正規化
void IMU9::normalizeQuaternion(float &q0, float &q1, float &q2, float &q3) {
  float norm = sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);
  if (norm < 0.001f) norm = 1.0f;
  q0 /= norm;
  q1 /= norm;
  q2 /= norm;
  q3 /= norm;
}

// 加速度からクォータニオンへ（重力ベクトル推定）
void IMU9::accelToQuaternion(float ax, float ay, float az, float &q0, float &q1, float &q2, float &q3) {
  // 加速度ベクトルを正規化
  float norm = sqrtf(ax*ax + ay*ay + az*az);
  if (norm < 0.001f) return;
  ax /= norm;
  ay /= norm;
  az /= norm;

  // 重力ベクトル(0, 0, -1)から現在の加速度方向への回転クォータニオン
  // TRIAD法: accから重力方向への回転
  float gx = 0.0f, gy = 0.0f, gz = -1.0f;  // 重力方向（下向き）

  // 外積: acc × gravity
  float cx = ay*gz - az*gy;
  float cy = az*gx - ax*gz;
  float cz = ax*gy - ay*gx;

  // 内積（回転角度の余弦）
  float dot = ax*gx + ay*gy + az*gz;
  
  // クォータニオン作成
  float s = sqrtf((1.0f + dot) * 2.0f);
  if (s < 0.001f) {
    // 反対方向の場合
    q0 = 0.0f;
    q1 = 1.0f;
    q2 = 0.0f;
    q3 = 0.0f;
  } else {
    q0 = s * 0.25f;
    q1 = cx / s;
    q2 = cy / s;
    q3 = cz / s;
  }

  normalizeQuaternion(q0, q1, q2, q3);
}

// ジャイロからのクォータニオン更新（積分）
void IMU9::updateQuaternionFromGyro(float &q0, float &q1, float &q2, float &q3, 
                                     float gx, float gy, float gz, float dt_s) {
  // ジャイロ角速度 [rad/s]
  // クォータニオン導関数: dq/dt = 0.5 * q * ω
  
  float half_dt = 0.5f * dt_s;
  
  // q' = q + 0.5 * dt * w * q
  float dq0 = (-q1*gx - q2*gy - q3*gz) * half_dt;
  float dq1 = ( q0*gx + q2*gz - q3*gy) * half_dt;
  float dq2 = ( q0*gy - q1*gz + q3*gx) * half_dt;
  float dq3 = ( q0*gz + q1*gy - q2*gx) * half_dt;
  
  q0 += dq0;
  q1 += dq1;
  q2 += dq2;
  q3 += dq3;
  
  normalizeQuaternion(q0, q1, q2, q3);
}

// 相補フィルター実行（加速度+ジャイロ融合）
void IMU9::updateQuaternion() {
  // 前回の更新時刻から経過時間を計算
  uint32_t now_ms = millis();
  float dt_s = (attitude_.last_update_ms > 0) 
    ? (now_ms - attitude_.last_update_ms) * 0.001f 
    : 0.005f;  // デフォルト200Hz
  
  if (dt_s > 0.1f) dt_s = 0.005f;  // 異常値チェック
  attitude_.last_update_ms = now_ms;

  // 加速度が有効な場合のみ処理
  float ax = latest_data_.Ax, ay = latest_data_.Ay, az = latest_data_.Az;
  float accel_norm_sq = ax*ax + ay*ay + az*az;
  
  if (accel_norm_sq > 0.001f) {
    attitude_.accel_trust = 1.0f;
  } else {
    attitude_.accel_trust = 0.0f;
  }

  // 初期化チェック
  if (!attitude_.initialized) {
    accelToQuaternion(ax, ay, az, attitude_.q0, attitude_.q1, attitude_.q2, attitude_.q3);
    attitude_.initialized = true;
    return;
  }

  // ジャイロで更新
  float gx_rad = latest_data_.Gx * M_PI / 180.0f;  // 角度→ラジアン
  float gy_rad = latest_data_.Gy * M_PI / 180.0f;
  float gz_rad = latest_data_.Gz * M_PI / 180.0f;

  updateQuaternionFromGyro(attitude_.q0, attitude_.q1, attitude_.q2, attitude_.q3, 
                           gx_rad, gy_rad, gz_rad, dt_s);

  // 加速度で補正（相補フィルター）
  if (attitude_.accel_trust > 0.1f) {
    float accel_q0, accel_q1, accel_q2, accel_q3;
    accelToQuaternion(ax, ay, az, accel_q0, accel_q1, accel_q2, accel_q3);
    
    // 融合ゲイン（加速度信頼度に応じた重み）
    float alpha = 0.1f * attitude_.accel_trust;  // 0.0～0.1
    
    // SLERP簡易版（低加重の場合は線形補間）
    attitude_.q0 += (accel_q0 - attitude_.q0) * alpha;
    attitude_.q1 += (accel_q1 - attitude_.q1) * alpha;
    attitude_.q2 += (accel_q2 - attitude_.q2) * alpha;
    attitude_.q3 += (accel_q3 - attitude_.q3) * alpha;
    
    normalizeQuaternion(attitude_.q0, attitude_.q1, attitude_.q2, attitude_.q3);
  }
}

}  // namespace component
