/*
 * IMU.h — 9軸IMU (BMI270 + BMM150) 姿勢推定 & サーボ姿勢制御ヘッダ
 *
 * ■ 概要
 *   ロケット搭載の ESP32-S3 でフィン姿勢制御を行うコンポーネント。
 *   加速度・ジャイロを相補フィルターで融合しクォータニオン姿勢を推定、
 *   PD制御でサーボを駆動して目標姿勢（真上 = ピッチ-90°）を維持する。
 *
 * ■ タイマー構成（すべて WOBC フレームワークの process::Timer）
 *   - QuaternionUpdateTimer : 50Hz  センサー読み取り＋相補フィルター姿勢積分
 *   - ServoControlTimer     : 20Hz  PD制御によるサーボ角度出力
 *   - IMUDataTimer          : 10Hz  テレメトリパケット生成・送信（LoRaは約1Hz）
 *
 * ■ 制御フロー
 *   1. 加速度ノルムが閾値超え → 発射検知
 *   2. CONTROL_DELAY_MS 経過後 → PD制御開始
 *   3. ピッチ or ヨー誤差が ATTITUDE_ERROR_ABORT_DEG を
 *      ATTITUDE_ERROR_ABORT_COUNT 回連続超過 → 制御終了（サーボ中立）
 */
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

// BMI2_BMM1_Class のグローバルインスタンス（platformio のライブラリ側で定義）
extern BMI2_BMM1_Class IMU;

namespace component {

// ==========================================================================
//  サーボ物理配線
//    SERVO_PIN_1 : GPIO4 — フィン1（ヨー軸制御）
//    SERVO_PIN_2 : GPIO5 — フィン2（ピッチ軸制御）
// ==========================================================================
constexpr int SERVO_PIN_1 = 4;
constexpr int SERVO_PIN_2 = 5;

// ==========================================================================
//  ESP32 LEDC PWM 設定
//    ・50Hz (= 20ms周期) のRCサーボ用PWM
//    ・14bit 分解能 → 16384 ステップ / 20000μs
//    ・パルス幅 500～2400μs → 角度 0～180°
// ==========================================================================
constexpr uint8_t SERVO1_LEDC_CH = 2;      // LEDC チャンネル（0,1はLEDで使用済み想定）
constexpr uint8_t SERVO2_LEDC_CH = 3;
constexpr uint16_t SERVO_FREQ = 50;        // PWM周波数 [Hz]
constexpr uint8_t SERVO_RES_BITS = 14;     // PWM分解能 [bit]
constexpr uint16_t SERVO_MIN_US = 500;     // 0° に対応するパルス幅 [μs]
constexpr uint16_t SERVO_MAX_US = 2400;    // 180° に対応するパルス幅 [μs]

// ==========================================================================
//  姿勢制御パラメータ
// ==========================================================================

// --- 目標姿勢クォータニオン ---
// q = (0.7071, 0, -0.7071, 0) はピッチ -90° = 機首が真上を向いた状態。
// q = (w, x, y, z) の表記。
constexpr float q0_target =  0.7071f;  // w
constexpr float q1_target =  0.0000f;  // x
constexpr float q2_target = -0.7071f;  // y
constexpr float q3_target =  0.0000f;  // z

// --- PD制御ゲイン ---
// ロール軸とピッチ軸で別々のゲインを使用。
// Kp : 比例ゲイン（現在の誤差に比例した出力）
// Kd : 微分ゲイン（誤差の変化速度に比例した出力、振動抑制）
constexpr float Kp_yaw   = 1.3f;   // ヨー比例ゲイン（フィン1 = servo1）
constexpr float Kd_yaw   = 0.35f;  // ヨー微分ゲイン（フィン1 = servo1）
constexpr float Kp_pitch = 1.5f;   // ピッチ比例ゲイン
constexpr float Kd_pitch = 0.45f;  // ピッチ微分ゲイン

// --- 制御パラメータ ---
constexpr float ERROR_DEADBAND = 0.01f;      // 誤差デッドバンド: これ以下の誤差は0とみなす（チャタリング防止）
constexpr float SERVO_MAX_STEP_DEG = 15.0f;  // スルーレート制限: 1制御周期あたりの最大角度変化量 [deg/cycle]
constexpr float DTERM_DT_MAX = 0.12f;        // D項を無効にする最大dt [s]（異常に長い周期ではD項がスパイクするため）

// --- 相補フィルター 加速度信頼度パラメータ ---
// 加速度データの信頼度を動的に変化させることで、
// ロケット加速時やノイズ時にジャイロ主体へ自動切替する。
constexpr float ACCEL_TRUST_MIN    = 0.15f;  // 信頼度の下限（0にすると完全にジャイロのみ→ドリフトする）
constexpr float ACCEL_TRUST_RISE   = 0.08f;  // 信頼度を上げる速度（1周期あたり +0.08）
constexpr float ACCEL_TRUST_FALL   = 0.30f;  // 信頼度を下げる速度（1周期あたり -0.30、急速遮断）
constexpr float ACCEL_REJECT_LOW_G  = 0.70f; // 信頼可能な加速度下限 [G]（自由落下に近いと信頼しない）
constexpr float ACCEL_REJECT_HIGH_G = 1.30f; // 信頼可能な加速度上限 [G]（大きな加速度では信頼しない）
constexpr float GYRO_REJECT_DPS    = 220.0f; // 角速度がこの値を超えると加速度を信頼しない [deg/s]
constexpr float MAHONY_KP          = 4.0f;   // Mahony式P補正ゲイン（重力方向への引き戻し強度、旧LERP alpha=0.08@50Hz相当）

// --- 発射検知・制御遷移 ---
constexpr float LAUNCH_DETECT_ACC_DEFAULT = 35.0f;  // デフォルト発射検知加速度閾値 [m/s²]（≒3.6G）
constexpr uint32_t CONTROL_DELAY_MS = 1000;          // 発射検知→制御開始までの遅延 [ms]（ランチャー離脱待ち）

// --- 姿勢誤差アボート ---
// ピッチまたはヨーの誤差角度が閾値を連続超過した場合、制御を終了してサーボを中立に戻す。
// 機体が回復不能な姿勢になった場合にフィンが暴れ続けるのを防止する安全機構。
constexpr float ATTITUDE_ERROR_ABORT_DEG = 20.0f;    // 誤差角度閾値 [deg]
constexpr uint32_t ATTITUDE_ERROR_ABORT_COUNT = 10;   // 連続超過回数（20Hz×10 = 0.5秒間ずっと超過したら終了）

// ==========================================================================
//  PIDController 構造体
//    D項（微分項）計算のために前回の誤差値を保持する。
// ==========================================================================
struct PIDController {
  float prev_error;  // 前回の制御周期での誤差値
};

// ==========================================================================
//  IMU9 クラス — 9軸IMUコンポーネント
//
//  process::Component を継承し、WOBC フレームワーク上でタイマー駆動される。
//  内部に3つのタイマー（姿勢積分・サーボ制御・テレメトリ）を持つ。
// ==========================================================================
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
    uint8_t unit_id_;
  } servo_control_timer_;

  // 発射検知加速度閾値 [m/s²]（コンストラクタで設定）
  float launch_acc_threshold_;

  // --- ユーティリティメソッド ---

  // クォータニオンを正規化（ノルム=1 にする）。NaNの場合は単位クォータニオンにリセット。
  static void normalizeQuaternion(float &q0, float &q1, float &q2, float &q3);

  // 加速度ベクトルから重力方向に基づく姿勢クォータニオンを計算（ヨー=0と仮定）。
  // 静止時の初期姿勢設定と、相補フィルターの加速度側クォータニオン生成に使用。
  static void accelToQuaternion(float ax, float ay, float az, float &q0, float &q1, float &q2, float &q3);

  // ジャイロ角速度 [rad/s] でクォータニオンを1ステップ積分更新する（1次オイラー法）。
  static void updateQuaternionFromGyro(float &q0, float &q1, float &q2, float &q3, float gx, float gy, float gz, float dt_s);

  // ESP32 LEDC PWMでサーボを初期化（チャンネル設定＋中立位置出力）。
  void initServoPwm();

  // 相補フィルターのメインループ: Mahony式重力ベクトル補正 + ジャイロ積分。
  void updateQuaternion();

  // PD制御のメインループ: 誤差クォータニオン→PD出力→スルーレート制限→サーボ出力。
  void runServoControl();

  // 角度[deg] → LEDCデューティ値に変換してサーボを駆動。
  void servoWriteAngle(uint8_t channel, int angle);
};
}