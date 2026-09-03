#include "FlightPin.h"

namespace component {
    FlightPin::FlightPin(uint8_t unit_id, int flight_pin, unsigned sample_freq_hz)
        : process::Component("FlightPin", component_id),
          flight_pin_(flight_pin),
          unit_id_(unit_id),
          sample_timer_(*this, unit_id_, sample_freq_hz > 0 ? 1000 / sample_freq_hz : 1000) {
    }

    void FlightPin::setup() {
        pinMode(flight_pin_, INPUT);
        pin_state_ = digitalRead(flight_pin_);
        pin_state_changed_ms_ = millis();
        start(sample_timer_);
    }

    FlightPin::SampleTimer::SampleTimer(FlightPin& flight_pin_ref, uint8_t unit_id_ref, unsigned interval_ms)
        : process::Timer("FlightPinTimer", interval_ms),
          flight_pin_(flight_pin_ref),
          unit_id_(unit_id_ref) {
    }

    void FlightPin::SampleTimer::callback() {
        // フライトピンの状態を取得
        const uint32_t now_ms = millis();
        const int pin_state = digitalRead(flight_pin_.flight_pin_);

        if (pin_state != flight_pin_.pin_state_) {
            flight_pin_.pin_state_ = pin_state;
            flight_pin_.pin_state_changed_ms_ = now_ms;
        }

        const uint32_t pin_state_elapsed_ms =
            now_ms - flight_pin_.pin_state_changed_ms_;

        // パケットを作成して送信
        wcpp::Packet packet = newPacket(64);
        packet.telemetry(telemetry_id, component_id(), flight_pin_.unit_id_, 0xFF,
                         kernel::nextPacketSequence(flight_pin_.unit_id_, 0xFF, component_id(),
                                                    wcpp::packet_type_mask | telemetry_id));
        packet.append("FP").setInt(pin_state);
        packet.append("ET").setInt(pin_state_elapsed_ms);
        packet.append("TS").setInt(now_ms); // タイムスタンプを追加
        sendPacket(packet);
    }
}
