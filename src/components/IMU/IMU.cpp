#include "IMU.h"

namespace component {

IMU9::IMU9(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz)
  : process::Component("IMU", IMU9::component_id),
    wire_(wire),
    IMU_(&::IMU),
    unit_id_(unit_id),
    imu_data_timer_(*this, IMU_, unit_id, 100),
    quaternion_update_timer_(*this),
    servo_control_timer_(*this, unit_id) {
}

void IMU9::setup() {
  if (!IMU_->begin()) {
    error("IMU", "Failed to initialize IMU");
  }

  start(quaternion_update_timer_);  // 100Hz センサー読み取り＋姿勢積分
  start(imu_data_timer_);           // 10Hz パケット生成・送信
}

IMU9::IMUDataTimer::IMUDataTimer(IMU9& IMU9_ref, BMI2_BMM1_Class* IMU_ref, uint8_t unit_id_ref, unsigned interval_ms)
  : process::Timer("IMUData", interval_ms),
    IMU9_(IMU9_ref), IMU_(IMU_ref), unit_id_(unit_id_ref) {
}

IMU9::QuaternionUpdateTimer::QuaternionUpdateTimer(IMU9& IMU9_ref)
  : process::Timer("QuaternionUpdate", 20),
    IMU9_(IMU9_ref) {
}

void IMU9::IMUDataTimer::callback() {
  // 最新のセンサーデータ・姿勢をスナップショット（100Hzタイマーで更新済み）
  float Ax = IMU9_.latest_data_.Ax;
  float Ay = IMU9_.latest_data_.Ay;
  float Az = IMU9_.latest_data_.Az;
  float Gx = IMU9_.latest_data_.Gx;
  float Gy = IMU9_.latest_data_.Gy;
  float Gz = IMU9_.latest_data_.Gz;
  float qx = IMU9_.attitude_.q1;
  float qy = IMU9_.attitude_.q2;
  float qz = IMU9_.attitude_.q3;
  float qw = IMU9_.attitude_.q0;
  uint32_t timestamp_ms = IMU9_.latest_data_.timestamp_ms;

  // 誤差クォータニオンの計算: q_err = q_target^-1 * q_current
  float q_err_w = q0_target*qw + q1_target*qx + q2_target*qy + q3_target*qz;
  float q_err_x = q0_target*qx - q1_target*qw - q2_target*qz + q3_target*qy;
  float q_err_y = q0_target*qy + q1_target*qz - q2_target*qw - q3_target*qx;
  float q_err_z = q0_target*qz - q1_target*qy + q2_target*qx - q3_target*qw;
  
  float q_err_norm = sqrtf(q_err_w*q_err_w + q_err_x*q_err_x + q_err_y*q_err_y + q_err_z*q_err_z);
  if (q_err_norm > 0.01f) {
    q_err_w /= q_err_norm;
    q_err_x /= q_err_norm;
    q_err_y /= q_err_norm;
    q_err_z /= q_err_norm;
  }
  
  float pitch_err_deg = 2.0f * q_err_y * 180.0f / M_PI;
  float yaw_err_deg = 2.0f * q_err_z * 180.0f / M_PI;
  
  wcpp::Packet packet = IMU9_.newPacket(80);
  packet.telemetry(IMU9::telemetry_id, IMU9::component_id, unit_id_, 0xFF, timestamp_ms);
  packet.append("Ts").setInt(timestamp_ms);
  packet.append("Ax").setFloat16(Ax);
  packet.append("Ay").setFloat16(Ay);
  packet.append("Az").setFloat16(Az);
  packet.append("Gx").setFloat16(Gx);
  packet.append("Gy").setFloat16(Gy);
  packet.append("Gz").setFloat16(Gz);
  packet.append("qx").setFloat16(qx);
  packet.append("qy").setFloat16(qy);
  packet.append("qz").setFloat16(qz);
  packet.append("qw").setFloat16(qw);
  packet.append("PE").setFloat16(pitch_err_deg);
  packet.append("YE").setFloat16(yaw_err_deg);

  // 10Hzのうち10回に1回だけLoRaへ流す（約1Hz）
  if (++lora_counter_ % 10 != 0) {
    packet.append("Im").setNull();  // SD保存のみ
  }

  IMU9_.sendPacket(packet);
}

IMU9::ServoControlTimer::ServoControlTimer(IMU9& IMU9_ref, uint8_t unit_id_ref)
  : process::Timer("ServoControl", 50),
    IMU9_(IMU9_ref), unit_id_(unit_id_ref) {
}

void IMU9::QuaternionUpdateTimer::callback() {
  // 100Hz でセンサー読み取り＋クォータニオン積分
  static uint32_t quat_count = 0;
  static uint32_t last_log_us = 0;
  static uint32_t last_call_us = 0;
  static uint32_t dt_min_us = 999999;
  static uint32_t dt_max_us = 0;

  uint32_t now_us = micros();
  if (last_call_us > 0) {
    uint32_t dt = now_us - last_call_us;
    if (dt < dt_min_us) dt_min_us = dt;
    if (dt > dt_max_us) dt_max_us = dt;
  }
  last_call_us = now_us;
  quat_count++;

  // 1秒ごとに実行回数・min/max間隔をログ出力（us単位）
  if (now_us - last_log_us >= 1000000) {
    IMU9_.log("IMU", __LINE__, "Hz=%u min=%u max=%u(us)", quat_count, dt_min_us, dt_max_us);
    quat_count = 0;
    dt_min_us = 999999;
    dt_max_us = 0;
    last_log_us = now_us;
  }

  // センサー読み取り（Available()チェック省略: BMI270 ODR=200Hz > 50Hz読み取り）
  // 磁気センサーはクォータニオン計算で未使用のため50Hzでは読み取らない
  float Ax = 0, Ay = 0, Az = 0;
  float Gx = 0, Gy = 0, Gz = 0;

  IMU9_.IMU_->readAcceleration(Ax, Ay, Az);
  IMU9_.IMU_->readGyroscope(Gx, Gy, Gz);

  // 最新データを更新
  IMU9_.latest_data_.Ax = Ax;
  IMU9_.latest_data_.Ay = Ay;
  IMU9_.latest_data_.Az = Az;
  IMU9_.latest_data_.Gx = Gx;
  IMU9_.latest_data_.Gy = Gy;
  IMU9_.latest_data_.Gz = Gz;
  IMU9_.latest_data_.timestamp_ms = millis();

  // 姿勢積分
  IMU9_.updateQuaternion();
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

  // ロール・ピッチをクォータニオンに変換 (ヨーは0と仮定)
  float roll  = atan2f(ay, az);
  float pitch = atan2f(-ax, sqrtf(ay*ay + az*az));
  
  float cr = cosf(roll * 0.5f);
  float sr = sinf(roll * 0.5f);
  float cp = cosf(pitch * 0.5f);
  float sp = sinf(pitch * 0.5f);
  
  q0 = cr * cp;
  q1 = sr * cp;
  q2 = cr * sp;
  q3 = -sr * sp;

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
  
  // 加速度信頼度：静止時（1G）から機動時まで段階的に判定
  // 約0.5～1.5G：信頼度高め、異常加速度時は落とす
  float accel_norm = sqrtf(accel_norm_sq);
  if (accel_norm > 0.1f && accel_norm < 2.0f) {
    attitude_.accel_trust = 1.0f;  // 0.5G～2.0G で信頼度100%
  } else if (accel_norm >= 2.0f) {
    attitude_.accel_trust = 0.5f;  // 大加速度時は50%減衰（ロケット衝撃時の過補正抑止）
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
    
    // 融合ゲイン：ロケット搭載で応答性重視
    // 加速度信頼度100%時に0.08（8%）～0.1（10%）で補正
    float alpha = 0.08f * attitude_.accel_trust;
    
    // SLERP簡易版（低加重の場合は線形補間）
    attitude_.q0 += (accel_q0 - attitude_.q0) * alpha;
    attitude_.q1 += (accel_q1 - attitude_.q1) * alpha;
    attitude_.q2 += (accel_q2 - attitude_.q2) * alpha;
    attitude_.q3 += (accel_q3 - attitude_.q3) * alpha;
    
    normalizeQuaternion(attitude_.q0, attitude_.q1, attitude_.q2, attitude_.q3);
  }
}

}  // namespace component
