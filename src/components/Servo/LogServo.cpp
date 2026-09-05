#include "LogServo.h"

Servo servo1;

namespace component {
    LogServo::SampleTimer::SampleTimer(LogServo& servo_ref, uint8_t unit_id_ref, unsigned sample_freq_hz)
        : process::Timer("LogServo", sample_freq_hz > 0 ? 1000 / sample_freq_hz : 1000),
          servo_(servo_ref),
          unit_id_(unit_id_ref),
          sample_freq_hz_(sample_freq_hz) {
    }

    LogServo::LogServo(uint8_t servo_pin, uint8_t adc_pin, uint8_t unit_id, uint8_t mode, unsigned sample_freq_hz)
        : process::Component("LogServo", component_id),
          servo_pin_(servo_pin),
          adc_pin_(adc_pin),
          mode_(mode),
          unit_id_(unit_id),
          sample_freq_hz_(sample_freq_hz),
          sample_timer_(*this, unit_id_, sample_freq_hz_) {
    }

    void LogServo::setup() {
        listener.command().packet('S');
        servo1.setPeriodHertz(50);
        servo1.attach(servo_pin_);
        pinMode(adc_pin_, INPUT);
        listen(listener, 16, true);
        start(sample_timer_);
    }

    void LogServo::setAngle(float angle) {
        servo1.write(angle);
    }

    void LogServo::SampleTimer::callback() {
        if (servo_.mode_ == WITH_READANGLE) {
            int adc_value = analogRead(servo_.adc_pin_);
            servo_.observedAngle = map(adc_value, 0, 4095, 0, 180);
        }

        if (servo_.listener) {
            wcpp::Packet command_packet = servo_.listener.pop();
            if (command_packet) {
                auto angle_entry = command_packet.find("An");
                if (angle_entry) {
                    servo_.setAngle((*angle_entry).getFloat32());
                }
            }

        }

        wcpp::Packet packet = newPacket(32);
        packet.telemetry(telemetry_id, component_id(), unit_id_, 0xFF, 1234);
        packet.append("An").setFloat32(servo_.observedAngle);
        sendPacket(packet);
    }
}