#pragma once

#include <library/wobc.h>


namespace component {
    class FlightPin: public process::Component {
    public:
        static const uint8_t component_id = 0x50;
        static const uint8_t telemetry_id = 'F';

        FlightPin(uint8_t unit_id, int flight_pin, unsigned sample_freq_hz = 1);

    protected:
        int flight_pin_;
        uint8_t unit_id_;
        int pin_state_ = LOW;
        uint32_t pin_state_changed_ms_ = 0;

        void setup() override;

        class SampleTimer: public process::Timer {
        public:
            SampleTimer(FlightPin& flight_pin_ref, uint8_t unit_id_ref, unsigned interval_ms);

        protected:
            void callback() override;

        private:
            FlightPin& flight_pin_;
            uint8_t unit_id_;
        } sample_timer_;
    };
    
}
