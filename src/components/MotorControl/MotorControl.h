#ifndef TACHOMETER_H
#define TACHOMETER_H

#include <Arduino.h>
#include <library/wobc.h>

namespace component {

class MotorControl : public process::Component {
private:
    uint8_t _ch1_pin;                     // センサー接続ピン
    uint8_t _ch2_pin;                     // センサー接続ピン
    static const uint8_t ch1_pwm_pin = 5;
    static const uint8_t ch2_pwm_pin = 6;
    static const uint8_t ch1_dir_pin = 7;
    static const uint8_t ch2_dir_pin = 8;
    uint16_t _set_rpm;                    // 設定RPM
    uint8_t _unit_id;                  // ユニットID
    static const uint8_t _telemetry_id = 'M';          // テレメトリID
    static const uint8_t _component_id = 0x48; // コンポーネントID
    
    volatile uint32_t _last_time;     // 最終検知時刻（マイクロ秒）
    volatile uint32_t _diff;          // パルス間隔（マイクロ秒）
    float _ch1current_rpm;               // 算出されたRPM
    float _ch2current_rpm;               // 算出されたRPM

    static void handleInterrupt();
    static MotorControl* _instance;

    static const uint16_t pwm_freq = 2000;
    static const uint16_t duty_max = 65535;
    const float p_gain = 20.0 * 2 / 3;
    const float i_gain = 0.2 * 2 / 3;
    const float d_gain = 0.0f;
    float ch1_average_rpm = 0.0f;
    float ch2_average_rpm = 0.0f;
    float ch1_error_integral = 0.0f;
    float ch2_error_integral = 0.0f;
    float ch1_previous_error = 0.0f;
    float ch2_previous_error = 0.0f;

public:
    MotorControl(uint8_t ch1_pin, uint8_t ch2_pin, uint8_t unit_id, uint16_t set_rpm, unsigned sample_freq_hz = 50);
    
    void begin();  
    void update(); 

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