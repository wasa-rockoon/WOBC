#include "MotorControl.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_arduino_version.h>
#endif

namespace component {

MotorControl* MotorControl::_instance = nullptr;

MotorControl::MotorControl(uint8_t ch1_pin, uint8_t ch2_pin, uint8_t unit_id, uint16_t set_rpm, unsigned sample_freq_hz)
  : process::Component("MotorControl", unit_id), 
    _ch1_pin(ch1_pin),
    _ch2_pin(ch2_pin),
    _set_rpm(set_rpm),
    _unit_id(unit_id),
    _ch1_last_time(0),
    _ch1_diff(0),
    _ch2_last_time(0),
    _ch2_diff(0),
    _sample_period_us(1000000UL / sample_freq_hz),
    _ch1current_rpm(0.0f),
    _ch2current_rpm(0.0f),
    sample_timer_(*this, _unit_id, 1000 / sample_freq_hz) {
}

MotorControl::SampleTimer::SampleTimer(MotorControl& motor_ref, uint8_t unit_id_ref, unsigned interval_ms)
    : process::Timer("MotorControl", interval_ms),
      motor_(motor_ref),
      unit_id_(unit_id_ref) {
}

void MotorControl::handleInterrupt_ch1() {
    if (_instance == nullptr) return;

    uint32_t current = micros();
    if (current - _instance->_ch1_last_time > 2000) {
        _instance->_ch1_diff = current - _instance->_ch1_last_time;
        _instance->_ch1_last_time = current;
    }
}

void MotorControl::handleInterrupt_ch2() {
    if (_instance == nullptr) return;

    uint32_t current = micros();
    if (current - _instance->_ch2_last_time > 2000) {
        _instance->_ch2_diff = current - _instance->_ch2_last_time;
        _instance->_ch2_last_time = current;
    }
}

void MotorControl::begin() {
    _instance = this;
    pinMode(_ch1_pin, INPUT_PULLUP);
    pinMode(_ch2_pin, INPUT_PULLUP);
    pinMode(ch1_pwm_1_pin, OUTPUT);
    pinMode(ch1_pwm_2_pin, OUTPUT);
    pinMode(ch2_pwm_1_pin, OUTPUT);
    pinMode(ch2_pwm_2_pin, OUTPUT);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(ch1_pwm_1_pin, pwm_freq, 16);
    ledcAttach(ch1_pwm_2_pin, pwm_freq, 16);
    ledcAttach(ch2_pwm_1_pin, pwm_freq, 16);
    ledcAttach(ch2_pwm_2_pin, pwm_freq, 16);
#else
    ledcSetup(ch1_pwm_channel, pwm_freq, 16);
    ledcSetup(ch2_pwm_channel, pwm_freq, 16);
    ledcAttachPin(ch1_pwm_1_pin, ch1_pwm_channel);
    ledcAttachPin(ch2_pwm_1_pin, ch2_pwm_channel);
#endif
    attachInterrupt(digitalPinToInterrupt(_ch1_pin), handleInterrupt_ch1, RISING);
    attachInterrupt(digitalPinToInterrupt(_ch2_pin), handleInterrupt_ch2, RISING);
    process::Component::begin();
}

void MotorControl::setup() {
    start(sample_timer_);
}

void MotorControl::update() {
    const float dt_s = _sample_period_us / 1000000.0f;
    const uint32_t now = micros();

    if (now - _ch1_last_time > 1000000UL) {
        _ch1_diff = 0;
        _ch1current_rpm = 0.0f;
    } else if (_ch1_diff > 0) {
        float rps = 1000000.0f / (float)_ch1_diff / 2.0f;
        _ch1current_rpm = rps * 60.0f;
    }

    if (now - _ch2_last_time > 1000000UL) {
        _ch2_diff = 0;
        _ch2current_rpm = 0.0f;
    } else if (_ch2_diff > 0) {
        float rps = 1000000.0f / (float)_ch2_diff / 2.0f;
        _ch2current_rpm = rps * 60.0f;
    }
    ch1_average_rpm = 0.9f * ch1_average_rpm + 0.1f * _ch1current_rpm;
    ch2_average_rpm = 0.9f * ch2_average_rpm + 0.1f * _ch2current_rpm;
    float ch1_error = _set_rpm - ch1_average_rpm;

    ch1_error_integral += ch1_error * dt_s;
    const float integral_limit = duty_max / i_gain;
    if (ch1_error_integral > integral_limit) ch1_error_integral = integral_limit;
    if (ch1_error_integral < -integral_limit) ch1_error_integral = -integral_limit;
    float ch1_derivative = (ch1_error - ch1_previous_error) / dt_s;
    float ch1_output = p_gain * ch1_error + i_gain * ch1_error_integral + d_gain * ch1_derivative;
    if (ch1_startup && ch1_average_rpm >= _set_rpm * startup_rpm_ratio) {
        ch1_startup = false;
    }
    if (ch1_startup && ch1_output > ch1_duty_command + startup_duty_step) {
        ch1_output = ch1_duty_command + startup_duty_step;
    }
    if (ch1_output < 0.0f) ch1_output = 0.0f;
    if (ch1_output > (ch1_startup ? startup_duty_limit : duty_max)) {
        ch1_output = ch1_startup ? startup_duty_limit : duty_max;
    }
    ch1_duty_command = ch1_output;
    uint16_t ch1_duty = (uint16_t)ch1_output;
    ch1_previous_error = ch1_error;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(ch1_pwm_1_pin, ch1_duty);
#else
    ledcWrite(ch1_pwm_channel, ch1_duty);
#endif

    float ch2_error = _set_rpm - ch2_average_rpm;
    ch2_error_integral += ch2_error * dt_s;
    if (ch2_error_integral > integral_limit) ch2_error_integral = integral_limit;
    if (ch2_error_integral < -integral_limit) ch2_error_integral = -integral_limit;
    float ch2_derivative = (ch2_error - ch2_previous_error) / dt_s;
    float ch2_output = p_gain * ch2_error + i_gain * ch2_error_integral + d_gain * ch2_derivative;
    if (ch2_startup && ch2_average_rpm >= _set_rpm * startup_rpm_ratio) {
        ch2_startup = false;
    }
    if (ch2_startup && ch2_output > ch2_duty_command + startup_duty_step) {
        ch2_output = ch2_duty_command + startup_duty_step;
    }
    if (ch2_output < 0.0f) ch2_output = 0.0f;
    if (ch2_output > (ch2_startup ? startup_duty_limit : duty_max)) {
        ch2_output = ch2_startup ? startup_duty_limit : duty_max;
    }
    ch2_duty_command = ch2_output;
    uint16_t ch2_duty = (uint16_t)ch2_output;
    ch2_previous_error = ch2_error;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(ch2_pwm_1_pin, ch2_duty);
#else
    ledcWrite(ch2_pwm_channel, ch2_duty);
#endif

    wcpp::Packet packet = newPacket(128);
    packet.telemetry(_telemetry_id, component_id(), _unit_id, 0xFF, 1234);
    packet.append("Ra").setFloat32(_ch1current_rpm);
    packet.append("Rb").setFloat32(_ch2current_rpm);
    packet.append("Da").setFloat32(ch1_duty);
    packet.append("Db").setFloat32(ch2_duty);
    packet.append("Ts").setInt((int)millis());
    
    sendPacket(packet);
}

void MotorControl::SampleTimer::callback() {
    motor_.update();
}

} // namespace component
