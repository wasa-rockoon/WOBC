#include <Arduino.h>
#include <library/wobc.h>

#if defined(ESP32)
  #include <ESP32Servo.h>
#else
  #include <Servo.h>
#endif

#define SERVO_ONLY 0
#define WITH_READANGLE 1

namespace component {
    class LogServo: public process::Component {
    public:
        static const uint8_t component_id = 0x47;
        static const uint8_t telemetry_id = 'S';
        LogServo(uint8_t servo_pin, uint8_t adc_pin, uint8_t unit_id, uint8_t mode = SERVO_ONLY, unsigned sample_freq_hz = 50);
        void setAngle(float angle);
        float observedAngle = 0.0f;

    private:
        uint8_t servo_pin_;
        uint8_t adc_pin_;
        uint8_t mode_;
        uint8_t unit_id_;
        unsigned sample_freq_hz_;
        Listener listener;

    protected:
        void setup() override;

    class SampleTimer: public process::Timer{
    public:
        SampleTimer(LogServo& servo_ref, uint8_t unit_id_ref, unsigned sample_freq_hz);
  
    protected:
        void callback() override;
    private:
        LogServo& servo_;
        uint8_t unit_id_;
        unsigned sample_freq_hz_;
    } sample_timer_;
    };
}