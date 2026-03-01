#ifndef ARDUINO
#define ARDUINO 100
#endif

#include <library/wobc.h>
#include <Arduino.h>
#include <Wire.h>
#include "BMI2_BMM1/BMI2_BMM1.h" //IMUセンサーのライブラリ。

// グローバル変数の外部宣言
extern BMI2_BMM1_Class IMU;

namespace component {

// ===== 参考コード準拠のピン配置 =====
constexpr int SERVO_PIN_1 = 4;
constexpr int SERVO_PIN_2 = 5;

// ===== LEDC PWM 設定 =====
constexpr uint8_t SERVO1_LEDC_CH = 2;
constexpr uint8_t SERVO2_LEDC_CH = 3;
constexpr uint16_t SERVO_FREQ = 50;
constexpr uint8_t SERVO_RES_BITS = 14;
constexpr uint16_t SERVO_MIN_US = 500;
constexpr uint16_t SERVO_MAX_US = 2400;

// ===== 姿勢制御パラメータ（参考コード準拠） =====
constexpr float q0_target = 0.1829f;
constexpr float q1_target = -0.7056f;
constexpr float q2_target = -0.1778f;
constexpr float q3_target = -0.6611f;

constexpr float Kp_roll = 1.0f;
constexpr float Kd_roll = 0.32f;
constexpr float Kp_pitch = 1.2f;
constexpr float Kd_pitch = 0.4f;
constexpr float ERROR_DEADBAND = 0.03f;
constexpr float SERVO_MAX_STEP_DEG = 8.0f;
constexpr float DTERM_DT_MAX = 0.12f;
constexpr float ACCEL_TRUST_MIN = 0.15f;

struct PIDController {
  float prev_error;
};

class IMU9: public process::Component {
public:
  static const uint8_t component_id = 0x30;
  static const uint8_t telemetry_id = 'I';

  IMU9(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz = 200);

protected:
  TwoWire& wire_;
  BMI2_BMM1_Class* IMU_;
  uint8_t unit_id_;

  // 最新のセンサーデータとクォータニオン結果
  struct {
    float Ax, Ay, Az, Gx, Gy, Gz, Mx, My, Mz;  // センサーデータ
    float qx, qy, qz, qw;  // クォータニオン
    uint32_t timestamp_ms;
    float servo1_deg, servo2_deg;
  } latest_data_;

  struct {
    float q0 = 1.0f;
    float q1 = 0.0f;
    float q2 = 0.0f;
    float q3 = 0.0f;
    bool initialized = false;
    uint32_t last_update_ms = 0;
    float accel_trust = 1.0f;
  } attitude_;

  struct {
    PIDController roll{0.0f};
    PIDController pitch{0.0f};
    float servo1_cmd = 90.0f;
    float servo2_cmd = 90.0f;
    uint32_t last_control_ms = 0;
  } servo_ctrl_;

  struct {
    uint32_t last_print_ms = 0;
  } debug_;

  void setup() override;

  // 200Hz IMUデータ取得タイマー
  class IMUDataTimer: public process::Timer{
  public:
    IMUDataTimer(IMU9& IMU9_ref, BMI2_BMM1_Class* IMU_ref, uint8_t unit_id_ref, unsigned interval_ms);
  
  protected:
    void callback() override;
  
  private:
    IMU9& IMU9_;
    BMI2_BMM1_Class* IMU_;
    uint8_t unit_id_;
  } imu_data_timer_;

  // 20Hz サーボ制御タイマー
  class ServoControlTimer: public process::Timer{
  public:
    ServoControlTimer(IMU9& IMU9_ref, uint8_t unit_id_ref);
  
  protected:
    void callback() override;
  
  private:
    IMU9& IMU9_;
    uint8_t unit_id_;
  } servo_control_timer_;

  static void normalizeQuaternion(float &q0, float &q1, float &q2, float &q3);
  static void accelToQuaternion(float ax, float ay, float az, float &q0, float &q1, float &q2, float &q3);
  static void updateQuaternionFromGyro(float &q0, float &q1, float &q2, float &q3, float gx, float gy, float gz, float dt_s);

  void initServoPwm();
  void updateQuaternion();
  void runServoControl();
  void servoWriteAngle(uint8_t channel, int angle);
};
}