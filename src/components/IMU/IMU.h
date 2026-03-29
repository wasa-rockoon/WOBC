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

#include <library/wobc.h>
#include <Arduino.h>
#include <Wire.h>
#include "BMI2_BMM1/BMI2_BMM1.h"  // BMI270(加速度+ジャイロ) + BMM150(地磁気) ドライバ

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
  static const uint8_t component_id = 0x30;   // WOBC コンポーネントID
  static const uint8_t telemetry_id = 'I';    // テレメトリパケットの識別子 'I' = IMU
  static const uint8_t raw_telemetry_id = 'R'; // 生データ(加速度・角速度)SD保存専用 50Hz

  // コンストラクタ
  // wire              : I2C バス（SDA=17, SCL=16）
  // unit_id           : ユニットID（パケットの宛先識別）
  // sample_freq_hz    : センサーサンプリング周波数（未使用、互換性のため残存）
  // launch_acc_threshold : 発射検知加速度閾値 [m/s²]
  IMU9(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz = 200, float launch_acc_threshold = LAUNCH_DETECT_ACC_DEFAULT);

protected:
  TwoWire& wire_;             // I2C バス参照
  BMI2_BMM1_Class* IMU_;      // BMI270+BMM150 センサードライバへのポインタ
  uint8_t unit_id_;            // WOBC ユニットID

  // --- 最新センサーデータ＆テレメトリ用スナップショット ---
  // QuaternionUpdateTimer(50Hz) が更新し、IMUDataTimer(10Hz)/ServoControlTimer(20Hz) が読む。
  struct {
    float Ax, Ay, Az;                // 加速度 [m/s²]（BMI270出力）
    float Gx, Gy, Gz;                // 角速度 [deg/s]（BMI270出力）
    float Mx, My, Mz;                // 地磁気 [μT]（BMM150出力、現在未使用）
    float qx, qy, qz, qw;           // 推定姿勢クォータニオン（テレメトリ送信用コピー）
    uint32_t timestamp_ms;            // データ取得時刻 [ms]
    float servo1_deg, servo2_deg;     // サーボ出力角度 [deg]（83°～97°、中立=90°）
    float pitch_error_deg;            // ピッチ誤差角度 [deg]（テレメトリ Ep）
    float yaw_error_deg;              // ヨー誤差角度 [deg]（テレメトリ Ey）
  } latest_data_;

  // --- 姿勢推定用クォータニオン状態 ---
  // 相補フィルターの内部状態。q0=w, q1=x, q2=y, q3=z。
  // 初期値 (1,0,0,0) = 回転なし。最初の加速度読み取りで初期姿勢を設定する。
  struct {
    float q0 = 1.0f;              // クォータニオン w 成分
    float q1 = 0.0f;              // クォータニオン x 成分
    float q2 = 0.0f;              // クォータニオン y 成分
    float q3 = 0.0f;              // クォータニオン z 成分
    bool initialized = false;      // 初回の加速度による姿勢初期化が完了したか
    uint32_t last_update_ms = 0;   // 前回更新時刻 [ms]（dt計算用）
    float accel_trust = 1.0f;      // 加速度の信頼度 [0.15～1.0]（相補フィルターの融合比率に使用）
  } attitude_;

  // --- サーボ PD制御の内部状態 ---
  struct {
    PIDController yaw{0.0f};       // ヨー軸 PD制御器（prev_error 保持、フィン1 = servo1）
    PIDController pitch{0.0f};     // ピッチ軸 PD制御器（prev_error 保持）
    float servo1_cmd = 90.0f;      // サーボ1 コマンド値 [deg]（スルーレート制限後）
    float servo2_cmd = 90.0f;      // サーボ2 コマンド値 [deg]（スルーレート制限後）
    uint32_t last_control_ms = 0;  // 前回制御時刻 [ms]（D項のdt計算用）
  } servo_ctrl_;

  // --- 発射検知・制御状態フラグ ---
  bool launch_detected_ = false;               // 発射を検知したか
  uint32_t launch_detection_time_ = 0;          // 発射検知時刻 [ms]
  bool control_aborted_ = false;                // 姿勢誤差超過で制御終了したか
  uint32_t attitude_error_exceed_count_ = 0;    // 姿勢誤差が連続で閾値を超えた回数

  // --- デバッグ用 ---
  struct {
    uint32_t last_print_ms = 0;
  } debug_;

  // setup() : IMUセンサー初期化→サーボPWM初期化→3つのタイマー開始
  void setup() override;

  // ====================================================================
  //  IMUDataTimer — テレメトリパケット生成・送信（10Hz）
  //
  //  最新の姿勢クォータニオン・サーボ角度・誤差角度・制御状態を
  //  wcpp::Packet に詰めて送信する。LoRa への送信は10回に1回（≒1Hz）。
  //
  //  パケットフィールド:
  //    Ts : タイムスタンプ [ms]
  //    qx, qy, qz, qw : 推定姿勢クォータニオン (Float16)
  //    S1, S2 : サーボ1/2 出力角度 [deg] (Int)
  //    Ep : ピッチ誤差角度 [deg] (Float16)
  //    Ey : ヨー誤差角度 [deg] (Float16)
  //    Ct : 制御状態 (Int) — 0:待機, 1:発射検知(遅延中), 2:制御中, 3:アボート済
  //    Im : LoRa送信抑制フラグ（nullならSD保存のみ）
  // ====================================================================
  class IMUDataTimer: public process::Timer{
  public:
    IMUDataTimer(IMU9& IMU9_ref, BMI2_BMM1_Class* IMU_ref, uint8_t unit_id_ref, unsigned interval_ms);
  
  protected:
    void callback() override;
  
  private:
    IMU9& IMU9_;
    BMI2_BMM1_Class* IMU_;
    uint8_t unit_id_;
    uint8_t lora_counter_ = 0;  // LoRa間引きカウンタ（10回に1回送信）
  } imu_data_timer_;

  // ====================================================================
  //  QuaternionUpdateTimer — センサー読み取り＋姿勢積分（50Hz）
  //
  //  BMI270から加速度・ジャイロを読み取り、相補フィルターで
  //  クォータニオン姿勢を更新する。
  //  ・ジャイロ: 高速応答（短期的に正確、長期的にドリフト）
  //  ・加速度 : 重力方向基準（長期的に正確、振動・加速に弱い）
  //  → Mahony式フィルターで両者を融合し互いの弱点を補う（ジンバルロックなし）。
  // ====================================================================
  class QuaternionUpdateTimer: public process::Timer{
  public:
    QuaternionUpdateTimer(IMU9& IMU9_ref);
  
  protected:
    void callback() override;
  
  private:
    IMU9& IMU9_;
  } quaternion_update_timer_;

  // ====================================================================
  //  ServoControlTimer — PD サーボ姿勢制御（20Hz）
  //
  //  現在姿勢と目標姿勢の誤差クォータニオンを計算し、
  //  PD制御でサーボ角度を算出する。
  //  発射検知→遅延→制御開始→誤差超過でアボート の状態遷移を管理。
  // ====================================================================
  class ServoControlTimer: public process::Timer{
  public:
    ServoControlTimer(IMU9& IMU9_ref, uint8_t unit_id_ref);
  
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