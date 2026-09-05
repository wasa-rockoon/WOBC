#ifndef TACHOMETER_H
#define TACHOMETER_H

#include <Arduino.h>
#include <library/wobc.h>

namespace component {

class MotorControl : public process::Component {
private:
    uint8_t _ch1_pin;                     // センサー接続ピン
    uint8_t _ch2_pin;                     // センサー接続ピン
    static const uint8_t ch1_pwm_1_pin = 4;
    static const uint8_t ch1_pwm_2_pin = 5;
    static const uint8_t ch2_pwm_1_pin = 6;
    static const uint8_t ch2_pwm_2_pin = 7;
    static const uint8_t ch1_pwm_channel = 0;
    static const uint8_t ch2_pwm_channel = 1;
    uint16_t _set_rpm;                    // 設定RPM
    uint8_t _unit_id;                  // ユニットID
    static const uint8_t _telemetry_id = 'M';          // テレメトリID
    static const uint8_t _component_id = 0x48; // コンポーネントID
    
    volatile uint32_t _ch1_last_time;     // ch1 最終検知時刻（マイクロ秒）
    volatile uint32_t _ch1_diff;           // ch1 パルス間隔（マイクロ秒）
    volatile uint32_t _ch2_last_time;     // ch2 最終検知時刻（マイクロ秒）
    volatile uint32_t _ch2_diff;           // ch2 パルス間隔（マイクロ秒）
    const uint32_t _sample_period_us;      // 更新周期（マイクロ秒）
    float _ch1current_rpm;                 // 算出されたRPM
    float _ch2current_rpm;                 // 算出されたRPM

    static void handleInterrupt_ch1();
    static void handleInterrupt_ch2();
    static MotorControl* _instance;

    static const uint16_t pwm_freq = 2000;
    static const uint16_t duty_max = 65535;
    static const uint16_t startup_duty_limit = duty_max / 3;
    static constexpr float startup_rpm_ratio = 0.8f;
    static constexpr float startup_duty_step = 500.0f;
    const float p_gain = 10.0 * 2 / 3;
    const float i_gain = 0.1 * 2 / 3;
    const float d_gain = 0.0f;
    float ch1_average_rpm = 0.0f;
    float ch2_average_rpm = 0.0f;
    float ch1_error_integral = 0.0f;
    float ch2_error_integral = 0.0f;
    float ch1_previous_error = 0.0f;
    float ch2_previous_error = 0.0f;
    float ch1_duty_command = 0.0f;
    float ch2_duty_command = 0.0f;
    bool ch1_startup = true;
    bool ch2_startup = true;

public:
    MotorControl(uint8_t ch1_pin, uint8_t ch2_pin, uint8_t unit_id, uint16_t set_rpm, unsigned sample_freq_hz = 50);
    
    void begin(); 
    void update();
    void setup() override;

class SampleTimer: public process::Timer {
    public:
        SampleTimer(MotorControl& motor_ref, uint8_t unit_id_ref, unsigned interval_ms);

    protected:
        void callback() override;

    private:
        MotorControl& motor_;
        uint8_t unit_id_;
    } sample_timer_;
};

} // namespace component

#endif
