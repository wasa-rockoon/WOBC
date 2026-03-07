/*
 * IMU.cpp — 9軸IMU (BMI270 + BMM150) 姿勢推定 & サーボ姿勢制御 実装
 *
 * ■ ファイル構成
 *   1. コンストラクタ / setup()
 *   2. IMUDataTimer      — テレメトリパケット生成・送信 (10Hz)
 *   3. QuaternionUpdateTimer — センサー読み取り＋相補フィルター姿勢積分 (50Hz)
 *   4. クォータニオンユーティリティ
 *      - normalizeQuaternion()    : 正規化 (NaNガード付き)
 *      - accelToQuaternion()      : 加速度→ロール/ピッチクォータニオン
 *      - updateQuaternionFromGyro(): ジャイロ1次オイラー積分
 *      - updateQuaternion()       : 相補フィルター(ジャイロ＋加速度LERP融合)
 *   5. サーボPWM制御
 *      - initServoPwm()    : LEDC PWM 初期化＋中立位置出力
 *      - servoWriteAngle() : 角度→LEDCデューティ変換
 *   6. runServoControl()   : PD姿勢制御メインループ (20Hz)
 */
#include "IMU.h"

namespace component {

// ======================================================================
//  1. コンストラクタ
// ======================================================================
// 初期化リストで WOBC フレームワークの Component 基底クラス、
// I2Cバス、BMI2_BMM1 ドライバ、3つの周期タイマーを初期化する。
// タイマーの実行間隔:
//   imu_data_timer_          : 100ms  = 10Hz   テレメトリ
//   quaternion_update_timer_ :  20ms  = 50Hz   姿勢積分
//   servo_control_timer_     :  50ms  = 20Hz   PD制御
IMU9::IMU9(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz, float launch_acc_threshold)
  : process::Component("IMU", IMU9::component_id),
    wire_(wire),
    IMU_(&::IMU),
    unit_id_(unit_id),
    imu_data_timer_(*this, IMU_, unit_id, 100),
    quaternion_update_timer_(*this),
    servo_control_timer_(*this, unit_id),
    launch_acc_threshold_(launch_acc_threshold) {
}

// ======================================================================
//  setup() — コンポーネント起動時に1回だけ呼ばれる
// ======================================================================
// 1. BMI270+BMM150 センサーを I2C で初期化
// 2. サーボPWM を LEDC で初期化し中立(90°)に設定
// 3. 3つの周期タイマーを登録・開始
void IMU9::setup() {
  if (!IMU_->begin()) {
    error("IMU", "Failed to initialize IMU");
  }

  initServoPwm();                    // サーボPWM初期化（LEDC設定＋中立位置出力）
  start(quaternion_update_timer_);   // 50Hz — センサー読み取り＋相補フィルター姿勢積分
  start(imu_data_timer_);            // 10Hz — テレメトリパケット生成・送信
  start(servo_control_timer_);       // 20Hz — PD制御によるサーボ角度出力
}

// ======================================================================
//  2. IMUDataTimer — テレメトリパケット生成・送信 (10Hz = 100ms周期)
// ======================================================================
// WOBC フレームワークの wcpp::Packet にセンサーデータを詰めて送信する。
// パケットは SD カードに保存され、10回に1回(≒1Hz)だけ LoRa にも送信される。
//
// パケットフィールド一覧:
//   Ts : タイムスタンプ [ms]           — センサーデータ取得時刻
//   qx, qy, qz, qw : 姿勢クォータニオン (Float16)
//   S1, S2 : サーボ1/2 出力角度 [deg]  — 83～97 の整数、中立=90
//   Ep : ピッチ誤差角度 [deg] (Float16) — 目標姿勢との誤差
//   Ey : ヨー誤差角度 [deg] (Float16)   — 目標姿勢との誤差
//   Ct : 制御状態 (Int)                — 0:待機, 1:発射検知(遅延中), 2:制御中, 3:アボート済
//   Im : LoRa送信抑制フラグ             — null→SD保存のみ、フィールド無し→LoRaにも送信
// ======================================================================

IMU9::IMUDataTimer::IMUDataTimer(IMU9& IMU9_ref, BMI2_BMM1_Class* IMU_ref, uint8_t unit_id_ref, unsigned interval_ms)
  : process::Timer("IMUData", interval_ms),
    IMU9_(IMU9_ref), IMU_(IMU_ref), unit_id_(unit_id_ref) {
}

IMU9::QuaternionUpdateTimer::QuaternionUpdateTimer(IMU9& IMU9_ref)
  : process::Timer("QuaternionUpdate", 20),  // 20ms = 50Hz
    IMU9_(IMU9_ref) {
}

void IMU9::IMUDataTimer::callback() {
  // --- 50Hzタイマーが更新した最新データのスナップショットを取得 ---
  // （排他制御なしだがESP32はシングルコアタスク想定なので問題なし）
  float Ax = IMU9_.latest_data_.Ax;
  float Ay = IMU9_.latest_data_.Ay;
  float Az = IMU9_.latest_data_.Az;
  float Gx = IMU9_.latest_data_.Gx;
  float Gy = IMU9_.latest_data_.Gy;
  float Gz = IMU9_.latest_data_.Gz;
  // 姿勢クォータニオンは attitude_ 構造体から直接取得
  // (q0=w, q1=x, q2=y, q3=z)
  float qx = IMU9_.attitude_.q1;
  float qy = IMU9_.attitude_.q2;
  float qz = IMU9_.attitude_.q3;
  float qw = IMU9_.attitude_.q0;
  uint32_t timestamp_ms = IMU9_.latest_data_.timestamp_ms;
  
  // --- パケット組み立て ---
  // newPacket(80) : 80バイトのバッファを確保してパケットを生成
  // telemetry()   : パケットヘッダにテレメトリIDやユニットIDを設定
  wcpp::Packet packet = IMU9_.newPacket(80);
  packet.telemetry(IMU9::telemetry_id, IMU9::component_id, unit_id_, 0xFF, timestamp_ms);
  packet.append("Ts").setInt(timestamp_ms);
  // 加速度・ジャイロは現在コメントアウト（パケットサイズ節約のため）
  /*
  packet.append("Ax").setFloat16(Ax);
  packet.append("Ay").setFloat16(Ay);
  packet.append("Az").setFloat16(Az);
  packet.append("Gx").setFloat16(Gx);
  packet.append("Gy").setFloat16(Gy);
  packet.append("Gz").setFloat16(Gz);
  */
  packet.append("qx").setFloat16(qx);
  packet.append("qy").setFloat16(qy);
  packet.append("qz").setFloat16(qz);
  packet.append("qw").setFloat16(qw);
  packet.append("S1").setInt((int)IMU9_.latest_data_.servo1_deg);  // サーボ1出力角度
  packet.append("S2").setInt((int)IMU9_.latest_data_.servo2_deg);  // サーボ2出力角度
  packet.append("Ep").setFloat16(IMU9_.latest_data_.pitch_error_deg);  // ピッチ誤差 [deg]
  packet.append("Ey").setFloat16(IMU9_.latest_data_.yaw_error_deg);   // ヨー誤差 [deg]

  // --- 制御状態フラグの判定 ---
  // 状態遷移: 0(待機) → 1(発射検知・遅延中) → 2(制御中) → 3(アボート済)
  // 一方向の遷移のみ（3になったら戻らない）
  uint8_t ctrl_state = 0;
  if (IMU9_.control_aborted_) {
    ctrl_state = 3;  // 姿勢誤差超過により制御終了
  } else if (IMU9_.launch_detected_ && (millis() - IMU9_.launch_detection_time_) >= CONTROL_DELAY_MS) {
    ctrl_state = 2;  // 遅延経過後 → PD制御実行中
  } else if (IMU9_.launch_detected_) {
    ctrl_state = 1;  // 発射検知済み、CONTROL_DELAY_MS 遅延待ち
  }
  packet.append("Ct").setInt(ctrl_state);

  // --- LoRa 送信間引き ---
  // 10Hz のうち10回に1回だけ LoRa に送信（≒1Hz）。
  // それ以外は "Im" を null に設定 → WOBC フレームワークが SD 保存のみ行う。
  if (++lora_counter_ % 10 != 0) {
    packet.append("Im").setNull();  // SD保存のみ、LoRa送信しない
  }

  IMU9_.sendPacket(packet);
}

// ======================================================================
//  ServoControlTimer コンストラクタ
//  50ms = 20Hz でサーボPD制御を実行
// ======================================================================
IMU9::ServoControlTimer::ServoControlTimer(IMU9& IMU9_ref, uint8_t unit_id_ref)
  : process::Timer("ServoControl", 50),  // 50ms = 20Hz
    IMU9_(IMU9_ref), unit_id_(unit_id_ref) {
}

// ======================================================================
//  3. QuaternionUpdateTimer コールバック (50Hz)
// ======================================================================
// BMI270 から加速度・ジャイロを読み取り、相補フィルターで姿勢を更新する。
// BMI270 の出力データレート(ODR) は 200Hz に設定されているため、
// 50Hz で読み取れば必ず新しいデータが得られる（Available() チェック不要）。
// 磁気センサー (BMM150) はクォータニオン計算で未使用のため読み取らない。
void IMU9::QuaternionUpdateTimer::callback() {
  float Ax = 0, Ay = 0, Az = 0;
  float Gx = 0, Gy = 0, Gz = 0;

  // BMI270 から加速度 [m/s²] とジャイロ [deg/s] を取得
  IMU9_.IMU_->readAcceleration(Ax, Ay, Az);
  IMU9_.IMU_->readGyroscope(Gx, Gy, Gz);

  // 最新データを latest_data_ に保存（テレメトリ送信用）
  IMU9_.latest_data_.Ax = Ax;
  IMU9_.latest_data_.Ay = Ay;
  IMU9_.latest_data_.Az = Az;
  IMU9_.latest_data_.Gx = Gx;
  IMU9_.latest_data_.Gy = Gy;
  IMU9_.latest_data_.Gz = Gz;
  IMU9_.latest_data_.timestamp_ms = millis();

  // 相補フィルターで姿勢クォータニオンを更新
  IMU9_.updateQuaternion();
}

// ServoControlTimer コールバック (20Hz)
// PD制御メインループを呼び出す
void IMU9::ServoControlTimer::callback() {
  IMU9_.runServoControl();
}

// ======================================================================
//  4. クォータニオンユーティリティ
// ======================================================================

// ----------------------------------------------------------------------
//  normalizeQuaternion() — クォータニオン正規化
// ----------------------------------------------------------------------
// 浮動小数点の蓄積誤差で |q| が 1 から離れるのを防ぐ。
// 毎回の姿勢積分後に呼び出して単位クォータニオンの条件 |q|=1 を維持する。
// NaN が含まれる場合はセンサー異常と判断し、安全な単位クォータニオン (1,0,0,0) にリセットする。
void IMU9::normalizeQuaternion(float &q0, float &q1, float &q2, float &q3) {
  // NaN チェック: ゼロ除算やオーバーフロー等で発生する可能性がある
  if (isnan(q0) || isnan(q1) || isnan(q2) || isnan(q3)) {
    q0 = 1.0f; q1 = 0.0f; q2 = 0.0f; q3 = 0.0f;
    return;
  }
  float norm = sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);
  if (norm > 0.0f) {
    q0 /= norm; q1 /= norm; q2 /= norm; q3 /= norm;
  } else {
    // ノルムが0（全成分0）の場合も単位クォータニオンにリセット
    q0 = 1.0f; q1 = 0.0f; q2 = 0.0f; q3 = 0.0f;
  }
}

// ----------------------------------------------------------------------
//  accelToQuaternion() — 加速度ベクトルからロール/ピッチクォータニオンを算出
// ----------------------------------------------------------------------
// 静止状態で加速度は重力方向のみを示すため、そこからロールとピッチを推定できる。
// ヨー（方位角）は加速度だけでは決定不能なため 0 と仮定する。
//
// 計算手順:
//   1. 加速度ベクトルを正規化して重力方向の単位ベクトルを得る
//   2. atan2 でロール角とピッチ角を算出
//   3. 半角の sin/cos からクォータニオンを組み立てる
//
// 用途:
//   - 初回起動時の姿勢初期化（updateQuaternion() 内 initialized==false のとき）
//   - 相補フィルターの加速度側クォータニオン（ジャイロとのLERP融合に使用）
void IMU9::accelToQuaternion(float ax, float ay, float az, float &q0, float &q1, float &q2, float &q3) {
  // 加速度ベクトルの正規化（ノルム→1のベクトル）
  float norm = sqrtf(ax*ax + ay*ay + az*az);
  if (norm < 0.001f) {
    // 加速度がほぼ0（自由落下中など）→ 方向が不定なので単位クォータニオン
    q0 = 1.0f; q1 = 0.0f; q2 = 0.0f; q3 = 0.0f;
    return;
  }
  ax /= norm;
  ay /= norm;
  az /= norm;

  // ロール角: Y軸とZ軸の加速度からatan2で計算 [rad]
  // ピッチ角: X軸（前方）の加速度と水平面成分からatan2で計算 [rad]
  // ヨーは加速度だけでは不定なので0とする
  float roll  = atan2f(ay, az);
  float pitch = atan2f(-ax, sqrtf(ay*ay + az*az));
  
  // 半角の三角関数からクォータニオンを生成（ZYX回転順序、ヨー=0）
  float cr = cosf(roll * 0.5f);   // cos(roll/2)
  float sr = sinf(roll * 0.5f);   // sin(roll/2)
  float cp = cosf(pitch * 0.5f);  // cos(pitch/2)
  float sp = sinf(pitch * 0.5f);  // sin(pitch/2)
  
  // クォータニオン = Qroll * Qpitch（ヨー=0なのでQyaw=単位クォータニオン）
  q0 = cr * cp;      // w
  q1 = sr * cp;      // x
  q2 = cr * sp;      // y
  q3 = -sr * sp;     // z

  normalizeQuaternion(q0, q1, q2, q3);
}

// ----------------------------------------------------------------------
//  updateQuaternionFromGyro() — ジャイロ角速度によるクォータニオン1次オイラー積分
// ----------------------------------------------------------------------
// クォータニオンの微分方程式:
//   dq/dt = 0.5 * q ⊗ ω   (ω = (0, gx, gy, gz) の純虚クォータニオン)
//
// 1次オイラー法で離散近似:
//   q(t+dt) = q(t) + dt * dq/dt
//
// dt_s が小さい（50Hz → 20ms）場合、1次近似で十分な精度が得られる。
// 積分後に normalizeQuaternion() でノルムを 1 に補正する。
//
// 引数:
//   gx, gy, gz : ジャイロ角速度 [rad/s]（呼び出し元で deg/s→rad/s 変換済み）
//   dt_s       : 時間ステップ [秒]
void IMU9::updateQuaternionFromGyro(float &q0, float &q1, float &q2, float &q3, 
                                     float gx, float gy, float gz, float dt_s) {
  float half_dt = 0.5f * dt_s;
  
  // クォータニオン微分方程式を展開して各成分の変化量を計算
  // dq0 = -0.5 * (q1*gx + q2*gy + q3*gz) * dt   ← スカラー部
  // dq1 =  0.5 * (q0*gx + q2*gz - q3*gy) * dt   ← x成分
  // dq2 =  0.5 * (q0*gy - q1*gz + q3*gx) * dt   ← y成分
  // dq3 =  0.5 * (q0*gz + q1*gy - q2*gx) * dt   ← z成分
  float dq0 = (-q1*gx - q2*gy - q3*gz) * half_dt;
  float dq1 = ( q0*gx + q2*gz - q3*gy) * half_dt;
  float dq2 = ( q0*gy - q1*gz + q3*gx) * half_dt;
  float dq3 = ( q0*gz + q1*gy - q2*gx) * half_dt;
  
  // オイラー積分: q(t+dt) = q(t) + dq
  q0 += dq0;
  q1 += dq1;
  q2 += dq2;
  q3 += dq3;
  
  // ノルムを1に再正規化（蓄積誤差の補正）
  normalizeQuaternion(q0, q1, q2, q3);
}

// ----------------------------------------------------------------------
//  updateQuaternion() — 相補フィルター メインループ
// ----------------------------------------------------------------------
// 相補フィルターは「ジャイロ積分」と「加速度による補正」を融合する手法。
//
// ジャイロの特性:
//   ○ 高速応答（振動の影響を受けにくい）
//   × 長時間でドリフト蓄積（積分誤差が溜まる）
//
// 加速度の特性 (重力方向基準):
//   ○ 長期的に安定（ドリフトしない）
//   × 機体加速・振動で瞬間的に不正確
//
// → 毎周期ジャイロで姿勢を更新し、Mahony式フィルターで重力方向を補正する。
//   オイラー角を経由しないため、ピッチ±90°でのジンバルロックが発生しない。
//   これにより短期的にはジャイロの応答を維持し、長期的にはドリフトを補正する。
//
// 加速度信頼度 (accel_trust) は条件により動的に変化する:
//   - 加速度ノルムが 0.7G～1.3G かつ 角速度が 220deg/s 以下 → 信頼度↑
//   - それ以外（ロケット加速中、高回転中）→ 信頼度↓
//   - 下限 0.15 を維持してジャイロドリフトを完全には放置しない
void IMU9::updateQuaternion() {
  // --- dt（経過時間）の計算 ---
  // 50Hz タイマーの設計値は 20ms だが、実際の経過時間を使う
  uint32_t now_ms = millis();
  float dt_s = (attitude_.last_update_ms > 0) 
    ? (now_ms - attitude_.last_update_ms) * 0.001f 
    : 0.02f;  // 初回はデフォルト 20ms (= 50Hz)
  
  // dt が異常に大きい場合はデフォルトに丸める（スリープ復帰時など）
  if (dt_s > 0.1f) dt_s = 0.02f;
  attitude_.last_update_ms = now_ms;

  // --- 加速度信頼度の動的調整 ---
  // ロケットの加速度環境に応じて、加速度データをどれだけ信頼するかを調整する。
  // 信頼度が高い（静止に近い）→ 加速度の補正を強く適用
  // 信頼度が低い（加速中・振動中）→ ジャイロ主体で制御
  float ax = latest_data_.Ax, ay = latest_data_.Ay, az = latest_data_.Az;
  float accel_norm_sq = ax*ax + ay*ay + az*az;
  
  constexpr float gravity = 9.80665f;  // 標準重力加速度 [m/s²]
  float accel_norm = sqrtf(accel_norm_sq);
  // 3軸ジャイロのうち最大の絶対値を取得（高回転判定用）
  float gyro_abs_max = fmaxf(fabsf(latest_data_.Gx), fmaxf(fabsf(latest_data_.Gy), fabsf(latest_data_.Gz)));
  
  // 信頼条件: (1) 加速度ノルムが 0.7G～1.3G の範囲内  AND  (2) 角速度が 220deg/s 以下
  // → 静止～低速飛行中は true、エンジン推力による大加速やスピン中は false
  bool accel_reliable = (accel_norm >= ACCEL_REJECT_LOW_G * gravity)
                     && (accel_norm <= ACCEL_REJECT_HIGH_G * gravity)
                     && (gyro_abs_max <= GYRO_REJECT_DPS);
  
  // 一次遅れフィルターで信頼度を滑らかに遷移させる
  // → 急激な信頼度変化による姿勢ジャンプを防止
  float target_trust = accel_reliable ? 1.0f : 0.0f;
  if (target_trust > attitude_.accel_trust) {
    attitude_.accel_trust += ACCEL_TRUST_RISE;   // +0.08/周期: ゆっくり上げる
  } else {
    attitude_.accel_trust -= ACCEL_TRUST_FALL;   // -0.30/周期: 素早く下げる（安全側）
  }
  // クランプ: [ACCEL_TRUST_MIN, 1.0] の範囲に収める
  // 下限 0.15 を維持することで、完全なジャイロのみ（ドリフト放置）を防ぐ
  attitude_.accel_trust = fmaxf(ACCEL_TRUST_MIN, fminf(1.0f, attitude_.accel_trust));

  // --- 初期姿勢の設定 ---
  // 起動後初回は加速度のみから姿勢クォータニオンを設定する。
  // ジャイロは相対変化しか分からないので、絶対的な初期姿勢が必要。
  if (!attitude_.initialized) {
    accelToQuaternion(ax, ay, az, attitude_.q0, attitude_.q1, attitude_.q2, attitude_.q3);
    attitude_.initialized = true;
    return;
  }

  // --- ステップ1: ジャイロ角速度 [deg/s] → [rad/s] ---
  float gx_rad = latest_data_.Gx * M_PI / 180.0f;
  float gy_rad = latest_data_.Gy * M_PI / 180.0f;
  float gz_rad = latest_data_.Gz * M_PI / 180.0f;

  // --- ステップ2: 加速度による重力ベクトル補正（Mahony式）---
  // 旧LERP方式では accelToQuaternion() 内でオイラー角（roll/pitch）を経由するため、
  // ピッチ±90°（＝目標姿勢そのもの！）でジンバルロックが発生し、
  // roll = atan2(~0, ~0) が不定 → ロール角がノイズで暴れ → 姿勢推定が崩壊していた。
  //
  // Mahony式はオイラー角を一切使わず、重力ベクトルの方向だけで補正する:
  //   1. 現在クォータニオンから「予測重力方向」を計算（回転行列の第3行）
  //   2. 実測加速度（正規化）との外積で誤差軸＋誤差量を得る
  //   3. 誤差量をジャイロ入力に加算してから積分
  // → オイラー角を経由しないので全姿勢で安定動作（ジンバルロックなし）
  if (attitude_.accel_trust > 0.1f) {
    float anorm = sqrtf(ax*ax + ay*ay + az*az);
    if (anorm > 0.001f) {
      // 測定加速度を正規化（重力方向の単位ベクトル）
      float mx = ax / anorm;
      float my = ay / anorm;
      float mz = az / anorm;

      // 現在クォータニオンから予測した重力方向（ボディフレーム）
      // 回転行列 R(q) の第3行 = ワールドZ軸（上方向）のボディフレーム表現
      // 静止時、正規化加速度 (mx,my,mz) と一致するはず
      float q0 = attitude_.q0, q1 = attitude_.q1;
      float q2 = attitude_.q2, q3 = attitude_.q3;
      float vx = 2.0f * (q1*q3 - q0*q2);
      float vy = 2.0f * (q2*q3 + q0*q1);
      float vz = q0*q0 - q1*q1 - q2*q2 + q3*q3;

      // 予測と実測の外積 = 補正すべき回転の軸と量
      // cross(measured, predicted) の方向にジャイロを補正すると
      // 予測重力が実測に近づく
      float ex = my*vz - mz*vy;
      float ey = mz*vx - mx*vz;
      float ez = mx*vy - my*vx;

      // Mahony P補正ゲイン
      // MAHONY_KP=4.0 は旧LERP方式の alpha=0.08 at 50Hz と等価な応答速度
      //   旧: alpha * error_per_step = 0.08 * θ
      //   新: Kp * |cross| * dt = 4.0 * θ * 0.02 = 0.08 * θ
      float kp = MAHONY_KP * attitude_.accel_trust;
      gx_rad += kp * ex;
      gy_rad += kp * ey;
      gz_rad += kp * ez;
    }
  }

  // --- ステップ3: 補正済みジャイロでクォータニオン積分 ---
  // ジャイロに重力補正を加えた後に積分することで、
  // 短期的にはジャイロの高速応答を維持しつつ、
  // 長期的には重力方向へのドリフト補正が掛かる。
  updateQuaternionFromGyro(attitude_.q0, attitude_.q1, attitude_.q2, attitude_.q3, 
                           gx_rad, gy_rad, gz_rad, dt_s);
}

// ======================================================================
//  5. サーボ PWM 制御
// ======================================================================

// ----------------------------------------------------------------------
//  initServoPwm() — ESP32 LEDC PWM を初期化しサーボを中立位置に設定
// ----------------------------------------------------------------------
// ESP32 の LEDC（LED制御）ペリフェラルを RC サーボ用 50Hz PWM として使う。
// ledcSetup()     : チャンネルの周波数と分解能を設定
// ledcAttachPin() : GPIO ピンをチャンネルに接続
// → 初期位置として 90°（中立）を出力
void IMU9::initServoPwm() {
  ledcSetup(SERVO1_LEDC_CH, SERVO_FREQ, SERVO_RES_BITS);  // ch2: 50Hz, 14bit
  ledcSetup(SERVO2_LEDC_CH, SERVO_FREQ, SERVO_RES_BITS);  // ch3: 50Hz, 14bit
  ledcAttachPin(SERVO_PIN_1, SERVO1_LEDC_CH);  // GPIO4 → ch2
  ledcAttachPin(SERVO_PIN_2, SERVO2_LEDC_CH);  // GPIO5 → ch3
  servoWriteAngle(SERVO1_LEDC_CH, 90);  // サーボ1を中立位置(90°)に設定
  servoWriteAngle(SERVO2_LEDC_CH, 90);  // サーボ2を中立位置(90°)に設定
  LOG("Servo PWM initialized.");
}

// ----------------------------------------------------------------------
//  servoWriteAngle() — 角度 [deg] → LEDC デューティ値に変換してサーボ駆動
// ----------------------------------------------------------------------
// RCサーボの信号仕様:
//   ・50Hz (20ms周期) の PWM
//   ・パルス幅 500μs = 0°、2400μs = 180°
// ESP32 LEDC の 14bit 分解能 (0～16383) で PWM を生成:
//   duty = pulseUs * 16384 / 20000
void IMU9::servoWriteAngle(uint8_t channel, int angle) {
  angle = constrain(angle, 0, 180);  // 安全のため 0～180° にクランプ
  // 角度 → パルス幅 [μs] に線形変換
  uint32_t pulseUs = map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
  // パルス幅 → 14bit LEDC デューティ値に変換
  // 20000μs(=50Hzの1周期) が 16384(=2^14) に対応
  uint32_t duty = (pulseUs * 16384UL) / 20000UL;
  ledcWrite(channel, duty);
}

// ======================================================================
//  6. runServoControl() — PD サーボ姿勢制御メインループ (20Hz = 50ms周期)
// ======================================================================
//
// ■ 全体フロー
//   (A) dt 計算＆D項有効判定
//   (B) 発射検知（加速度ノルム閾値超え）
//   (C) 制御有効判定（発射後 + 遅延経過 + 未アボート）
//   (D) 誤差クォータニオン計算: q_err = q_target* ⊗ q_current
//   (E) 最短経路補正 (q_err_w < 0 なら符号反転)
//   (F) 姿勢誤差アボート判定（20°超を10回連続→制御終了）
//   (G) PD制御出力計算
//   (H) スルーレート制限（1周期あたり最大15°変化）
//   (I) 角度制限（±7°＝83°～97°）
//   (J) サーボPWM出力
//
// ■ 誤差クォータニオンの数学
//   q_err = conj(q_target) ⊗ q_current
//   = 「目標姿勢から見た現在姿勢の相対回転」
//   q_err_w ≈ cos(θ/2) ≈ 1 のとき姿勢が一致
//   q_err_x,y,z ≈ sin(θ/2) * 回転軸 ≈ 0 のとき姿勢が一致
//
//   小アングル近似（θが小さいとき sin(θ/2) ≈ θ/2）:
//     誤差角度 [rad] ≈ 2 * q_err_component
//     誤差角度 [deg] = 2 * q_err_component * 180/π
void IMU9::runServoControl() {

  // ========== (A) dt 計算 ==========
  // D項（微分項）の計算に使う経過時間。
  // 初回（last_control_ms == 0）はデフォルト 50ms を使用。
  uint32_t now_ms = millis();
  float dt = (servo_ctrl_.last_control_ms == 0)
    ? 0.05f
    : (now_ms - servo_ctrl_.last_control_ms) * 0.001f;
  servo_ctrl_.last_control_ms = now_ms;
  // dt が異常（0以下 or 120ms超）の場合はD項を無効化
  // → 制御開始直後やタイマー遅延時にD項がスパイクするのを防止
  bool disable_dterm = (dt <= 0.0f) || (dt > DTERM_DT_MAX);

  // ========== (B) 発射検知 ==========
  // 加速度ベクトルのノルム（3軸合成加速度）が閾値を超えたら発射と判定。
  // 一度検知したら以降はこのブロックをスキップ（launch_detected_ = true のまま）。
  if (!launch_detected_) {
    float ax = latest_data_.Ax, ay = latest_data_.Ay, az = latest_data_.Az;
    float acc_norm = sqrtf(ax*ax + ay*ay + az*az);
    if (acc_norm >= launch_acc_threshold_) {
      launch_detected_ = true;
      launch_detection_time_ = now_ms;  // 発射時刻を記録（遅延計算の起点）
      LOG("Launch detected! acc=%.1f m/s^2", acc_norm);
    }
  }

  // ========== (C) 制御有効判定 ==========
  // 以下の3条件がすべて真のとき PD 制御をアクティブにする:
  //   1. 発射が検知されている
  //   2. 姿勢誤差アボートが発生していない
  //   3. 発射から CONTROL_DELAY_MS (1500ms) が経過している
  //      → ランチャー離脱後にフィンが暴れるのを防ぐための待ち時間
  bool control_active = launch_detected_
                     && !control_aborted_
                     && (now_ms - launch_detection_time_) >= CONTROL_DELAY_MS;

  // クォータニオンの妥当性チェック（NaN や異常な値を弾く）
  float q0 = attitude_.q0, q1 = attitude_.q1;
  float q2 = attitude_.q2, q3 = attitude_.q3;
  float qnorm = sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);
  bool quat_valid = !isnan(qnorm) && (qnorm >= 0.1f);

  // サーボ目標角度（制御無効時は中立 90° を目指す）
  float servo1_target = 90.0f;
  float servo2_target = 90.0f;

  // ========== 制御有効 かつ クォータニオン正常なら PD 制御実行 ==========
  if (control_active && quat_valid) {

    // ====== (D) 誤差クォータニオンの計算 ======
    // q_err = conj(q_target) ⊗ q_current
    // 目標姿勢の共役（逆回転）に現在姿勢を掛けることで、
    // 「目標姿勢から現在姿勢への相対回転」を得る。
    // 姿勢が一致していれば q_err = (1, 0, 0, 0)。
    //
    // クォータニオン積の公式 (Hamilton積):
    //   (a,b,c,d) ⊗ (e,f,g,h) = (
    //     ae-bf-cg-dh,
    //     af+be+ch-dg,
    //     ag-bh+ce+df,
    //     ah+bg-cf+de )
    // ※ conj(q_target) = (w, -x, -y, -z) なので符号に注意
    float q_err_w = q0_target*q0 + q1_target*q1 + q2_target*q2 + q3_target*q3;
    float q_err_x = q0_target*q1 - q1_target*q0 - q2_target*q3 + q3_target*q2;
    float q_err_y = q0_target*q2 + q1_target*q3 - q2_target*q0 - q3_target*q1;
    float q_err_z = q0_target*q3 - q1_target*q2 + q2_target*q1 - q3_target*q0;

    // 誤差クォータニオンを正規化
    float q_err_norm = sqrtf(q_err_w*q_err_w + q_err_x*q_err_x + q_err_y*q_err_y + q_err_z*q_err_z);
    if (q_err_norm > 0.01f) {
      q_err_w /= q_err_norm;
      q_err_x /= q_err_norm;
      q_err_y /= q_err_norm;
      q_err_z /= q_err_norm;
    }

    // ====== (E) 最短経路補正 ======
    // クォータニオン q と -q は同じ回転を表す（二重被覆性）。
    // q_err_w < 0 だと 180°超の遠回り経路を示すため、符号を反転して
    // 最短経路（180°以内）の回転に変換する。
    // これがないと、90°の誤差が 270° と誤認されて誤差が 0° に見える問題が発生する。
    if (q_err_w < 0.0f) {
      q_err_w = -q_err_w;
      q_err_x = -q_err_x;
      q_err_y = -q_err_y;
      q_err_z = -q_err_z;
    }

    // ====== (F) 姿勢誤差アボート判定 ======
    // 小アングル近似: 誤差角度 [deg] ≈ 2 * q_err_component * (180/π)
    // ピッチまたはヨーが 20° を超える場合、機体は回復困難と判断。
    // 10回連続超過 (= 20Hz × 10 = 0.5秒間) でアボート → サーボ中立に固定。
    float pitch_error_deg = 2.0f * q_err_y * (180.0f / M_PI);
    float yaw_error_deg   = 2.0f * q_err_z * (180.0f / M_PI);
    // テレメトリ送信用に保存（符号付き: 正/負で方向がわかる）
    latest_data_.pitch_error_deg = pitch_error_deg;
    latest_data_.yaw_error_deg   = yaw_error_deg;

    if (fabsf(pitch_error_deg) > ATTITUDE_ERROR_ABORT_DEG || fabsf(yaw_error_deg) > ATTITUDE_ERROR_ABORT_DEG) {
      // 閾値超過 → カウンタ加算
      attitude_error_exceed_count_++;
      if (attitude_error_exceed_count_ >= ATTITUDE_ERROR_ABORT_COUNT) {
        // 連続超過回数が上限に到達 → 制御終了
        control_aborted_ = true;
        LOG("Control aborted! pitch_err=%.1fdeg yaw_err=%.1fdeg (exceeded %u consecutive times)",
            pitch_error_deg, yaw_error_deg, (unsigned)ATTITUDE_ERROR_ABORT_COUNT);
        // サーボを中立位置に戻して安全停止
        servoWriteAngle(SERVO1_LEDC_CH, 90);
        servoWriteAngle(SERVO2_LEDC_CH, 90);
        servo_ctrl_.servo1_cmd = 90.0f;
        servo_ctrl_.servo2_cmd = 90.0f;
        latest_data_.servo1_deg = 90.0f;
        latest_data_.servo2_deg = 90.0f;
        return;  // 以降のPD制御をスキップ
      }
    } else {
      // 閾値以下に戻ったらカウンタをリセット（一時的な外乱を許容）
      attitude_error_exceed_count_ = 0;
    }

    // ====== (G) PD制御出力計算 ======
    // 小アングル近似で誤差ベクトルを取得:
    //   error = 2 * q_err_component [rad] (無次元)
    // q_err_z → ヨー軸誤差（フィン1で補正）、q_err_y → ピッチ軸誤差（フィン2で補正）
    // ※ ロール（q_err_x）は2枚フィンでは制御不能なので使用しない
    float error_x = 2.0f * q_err_z;  // ヨー誤差 [rad相当]（フィン1で補正）
    float error_y = 2.0f * q_err_y;  // ピッチ誤差 [rad相当]（フィン2で補正）

    // デッドバンド: 微小な誤差を0にしてサーボのチャタリングを防止
    if (fabsf(error_x) < ERROR_DEADBAND) error_x = 0.0f;
    if (fabsf(error_y) < ERROR_DEADBAND) error_y = 0.0f;

    // --- PD制御 — ヨー軸（フィン1 = servo1） ---
    // P項: 比例ゲイン × 現在の誤差（誤差が大きいほど強く修正）
    // D項: 微分ゲイン × 誤差の変化量（変化が速いほど強くブレーキ、振動抑制）
    float d_error_x = disable_dterm ? 0.0f : (error_x - servo_ctrl_.yaw.prev_error);
    float output_x = Kp_yaw * error_x + Kd_yaw * d_error_x;
    output_x = fmaxf(-1.0f, fminf(1.0f, output_x));  // [-1, +1] にクランプ
    servo_ctrl_.yaw.prev_error = error_x;  // 次回のD項計算用に保存

    // --- PD制御 — ピッチ軸 ---
    float d_error_y = disable_dterm ? 0.0f : (error_y - servo_ctrl_.pitch.prev_error);
    float output_y = Kp_pitch * error_y + Kd_pitch * d_error_y;
    output_y = fmaxf(-1.0f, fminf(1.0f, output_y));  // [-1, +1] にクランプ
    servo_ctrl_.pitch.prev_error = error_y;

    // --- PD出力 → サーボ角度に変換 ---
    // output [-1, +1] × max_deflection(20°) → ±20° の偏差角
    // 中立(90°) を基準に加減算
    // servo1_dir/servo2_dir はサーボの取り付け方向に合わせた符号
    constexpr float max_deflection = 20.0f;  // PD出力±1.0時の舵角 [deg]
    constexpr int servo1_dir = -1;  // サーボ1の回転方向（ヨー軸）
    constexpr int servo2_dir = +1;  // サーボ2の回転方向（ピッチ軸）
    servo1_target = 90.0f + servo1_dir * max_deflection * output_x;
    servo2_target = 90.0f + servo2_dir * max_deflection * output_y;

  } else {
    // ========== 制御無効中の処理 ==========
    // 目標角度は中立(90°)のままだが、PD制御器のprev_errorを
    // 現在の誤差で追従させておく。
    // → 制御開始時にD項が急激なスパイクを出すのを防止
    //   （prev_error=0 のまま制御開始すると、最初の1周期で
    //    d_error が大きくなりサーボが暴れる）
    if (quat_valid) {
      // 制御有効時と同じ方法で誤差クォータニオンを計算
      float q_err_w = q0_target*q0 + q1_target*q1 + q2_target*q2 + q3_target*q3;
      float q_err_x = q0_target*q1 - q1_target*q0 - q2_target*q3 + q3_target*q2;
      float q_err_y = q0_target*q2 + q1_target*q3 - q2_target*q0 - q3_target*q1;
      float q_err_z = q0_target*q3 - q1_target*q2 + q2_target*q1 - q3_target*q0;
      float q_err_norm = sqrtf(q_err_w*q_err_w + q_err_x*q_err_x + q_err_y*q_err_y + q_err_z*q_err_z);
      if (q_err_norm > 0.01f) {
        q_err_w /= q_err_norm;
        q_err_x /= q_err_norm;
        q_err_y /= q_err_norm;
        q_err_z /= q_err_norm;
      }
      // 最短経路補正（制御有効時と同じ処理）
      if (q_err_w < 0.0f) {
        q_err_w = -q_err_w;
        q_err_x = -q_err_x;
        q_err_y = -q_err_y;
        q_err_z = -q_err_z;
      }
      // prev_error を更新（D項スパイク防止のため）
      servo_ctrl_.yaw.prev_error   = 2.0f * q_err_z;  // ヨー誤差（フィン1用）
      servo_ctrl_.pitch.prev_error = 2.0f * q_err_y;  // ピッチ誤差（フィン2用）
      // テレメトリ用に誤差角度を保存（制御無効中も表示するため）
      latest_data_.pitch_error_deg = 2.0f * q_err_y * (180.0f / M_PI);
      latest_data_.yaw_error_deg   = 2.0f * q_err_z * (180.0f / M_PI);
    } else {
      // クォータニオンが異常な場合は誤差0として報告
      latest_data_.pitch_error_deg = 0.0f;
      latest_data_.yaw_error_deg   = 0.0f;
    }
  }

  // ========== (H) スルーレート制限 ==========
  // 1制御周期あたりの角度変化量を SERVO_MAX_STEP_DEG (15°) 以下に制限。
  // サーボモーターの機械的な追従限界を超えないようにし、
  // 急激な角度変化によるギア損傷やオーバーシュートを防止する。
  // 制御無効時も適用（中立に戻るときにもゆっくり動かす）。
  float delta1 = servo1_target - servo_ctrl_.servo1_cmd;
  if (delta1 >  SERVO_MAX_STEP_DEG) delta1 =  SERVO_MAX_STEP_DEG;
  if (delta1 < -SERVO_MAX_STEP_DEG) delta1 = -SERVO_MAX_STEP_DEG;
  servo_ctrl_.servo1_cmd += delta1;

  float delta2 = servo2_target - servo_ctrl_.servo2_cmd;
  if (delta2 >  SERVO_MAX_STEP_DEG) delta2 =  SERVO_MAX_STEP_DEG;
  if (delta2 < -SERVO_MAX_STEP_DEG) delta2 = -SERVO_MAX_STEP_DEG;
  servo_ctrl_.servo2_cmd += delta2;

  // ========== (I) 角度制限 ==========
  // サーボの物理的な可動範囲を 83°～97°（中立90° ± 7°）に制限。
  // max_deflection(20°) は PD 出力の正規化係数であり、
  // 最終的にはこの ±7° でクランプされるので実際の最大舵角は ±7°。
  constexpr float max_deflection_limit = 7.0f;
  constexpr int angle_min = 90 - (int)max_deflection_limit;  // = 83°
  constexpr int angle_max = 90 + (int)max_deflection_limit;  // = 97°
  int angle1 = constrain((int)servo_ctrl_.servo1_cmd, angle_min, angle_max);
  int angle2 = constrain((int)servo_ctrl_.servo2_cmd, angle_min, angle_max);

  // ========== (J) サーボ PWM 出力 ==========
  servoWriteAngle(SERVO1_LEDC_CH, angle1);
  servoWriteAngle(SERVO2_LEDC_CH, angle2);

  // テレメトリ送信用に最終出力角度を保存
  latest_data_.servo1_deg = (float)angle1;
  latest_data_.servo2_deg = (float)angle2;
}

}  // namespace component
