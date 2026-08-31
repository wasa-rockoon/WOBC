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
    _last_time(0),
    _diff(0),
    _ch1current_rpm(0.0f),
    _ch2current_rpm(0.0f),
    sample_timer_(*this, _unit_id, 1000 / sample_freq_hz) {
}

MotorControl::SampleTimer::SampleTimer(MotorControl& motor_ref, uint8_t unit_id_ref, unsigned interval_ms)
    : process::Timer("MotorControl", interval_ms),
      motor_(motor_ref),
      unit_id_(unit_id_ref) {
}

void MotorControl::handleInterrupt() {
    if (_instance == nullptr) return;
    
    uint32_t current = micros();
    if (current - _instance->_last_time > 2000) {
        _instance->_diff = current - _instance->_last_time;
        _instance->_last_time = current;
    }
}

void MotorControl::begin() {
    pinMode(_ch1_pin, INPUT_PULLUP);
    pinMode(_ch2_pin, INPUT_PULLUP);
    pinMode(ch1_dir_pin, OUTPUT);
    pinMode(ch2_dir_pin, OUTPUT);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(ch1_pwm_pin, pwm_freq, 16);
    ledcAttach(ch2_pwm_pin, pwm_freq, 16);
#else
    ledcSetup(ch1_pwm_channel, pwm_freq, 16);
    ledcSetup(ch2_pwm_channel, pwm_freq, 16);
    ledcAttachPin(ch1_pwm_pin, ch1_pwm_channel);
    ledcAttachPin(ch2_pwm_pin, ch2_pwm_channel);
#endif
    attachInterrupt(digitalPinToInterrupt(_ch1_pin), handleInterrupt, RISING);
    attachInterrupt(digitalPinToInterrupt(_ch2_pin), handleInterrupt, RISING);
}

void MotorControl::update() {
    uint32_t now = micros();
    uint32_t dt = now - _last_time;

    if (dt > 1000000) {
        _diff = 0;
        _ch1current_rpm = 0.0f;
        _ch2current_rpm = 0.0f;
    } 
    else if (_diff > 0) {
        float rps = 1000000.0f / (float)_diff / 2.0f;
        _ch1current_rpm = rps * 60.0f;
        _ch2current_rpm = rps * 60.0f;
    }
    ch1_average_rpm = 0.9f * ch1_average_rpm + 0.1f * _ch1current_rpm;
    ch2_average_rpm = 0.9f * ch2_average_rpm + 0.1f * _ch2current_rpm;
    float ch1_error = _set_rpm - ch1_average_rpm;

    ch1_error_integral += ch1_error * dt;
    float ch1_derivative = (ch1_error - ch1_previous_error) / dt;
    uint16_t ch1_duty = (uint16_t)(p_gain * ch1_error + i_gain * ch1_error_integral + d_gain * ch1_derivative);
    ch1_previous_error = ch1_error;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(ch1_pwm_pin, ch1_duty);
#else
    ledcWrite(ch1_pwm_channel, ch1_duty);
#endif

    float ch2_error = _set_rpm - ch2_average_rpm;
    ch2_error_integral += ch2_error * dt;
    float ch2_derivative = (ch2_error - ch2_previous_error) / dt;
    uint16_t ch2_duty = (uint16_t)(p_gain * ch2_error + i_gain * ch2_error_integral + d_gain * ch2_derivative);
    ch2_previous_error = ch2_error;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(ch2_pwm_pin, ch2_duty);
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
    digitalWrite(ch1_dir_pin, HIGH); // Set direction for channel 1
    digitalWrite(ch2_dir_pin, HIGH); // Set direction for channel 2
    motor_.update();
}

} // namespace component
