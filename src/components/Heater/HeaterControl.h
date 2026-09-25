#pragma once

#include <cmath>
#include <cstdint>

namespace component {

// CH1 only. The caller serializes sampling and output-task access.
class HeaterControl {
public:
    static constexpr uint32_t TEMP_TIMEOUT_MS = 2000;
    static constexpr unsigned RECOVERY_SAMPLES = 3;
    enum class State { On, Off, LowBattery, Stale, Invalid, Recovering };

    void observe(float temperature, uint32_t now) {
        if (!std::isfinite(temperature)) {
            invalidate();
            return;
        }
        if (!valid_ || uint32_t(now - last_valid_ms_) >= TEMP_TIMEOUT_MS) {
            consecutive_ = 0;
        }
        temperature_ = temperature;
        last_valid_ms_ = now;
        valid_ = true;
        invalid_ = false;
        if (consecutive_ < RECOVERY_SAMPLES) ++consecutive_;
    }

    void missing() {
        // Tolerate brief communication failures while running, but recovery
        // must consist of consecutive successful measurements.
        if (consecutive_ < RECOVERY_SAMPLES) consecutive_ = 0;
    }

    void invalidate() {
        valid_ = false;
        invalid_ = true;
        consecutive_ = 0;
    }

    State evaluate(uint32_t now, int battery_mv) {
        if (invalid_) return State::Invalid;
        if (!valid_ || uint32_t(now - last_valid_ms_) >= TEMP_TIMEOUT_MS) {
            consecutive_ = 0;
            return State::Stale;
        }
        if (battery_mv < 6400) return State::LowBattery;
        if (consecutive_ < RECOVERY_SAMPLES) return State::Recovering;
        return temperature_ < 40.0f ? State::On : State::Off;
    }

    static const char* status(State state) {
        switch (state) {
            case State::On: return "ON";
            case State::LowBattery: return "OFF_LOW_BATT";
            case State::Stale: return "OFF_TEMP_STALE";
            case State::Invalid: return "OFF_TEMP_INVALID";
            case State::Recovering: return "OFF_TEMP_RECOVER";
            default: return "OFF";
        }
    }

private:
    float temperature_ = 0;
    uint32_t last_valid_ms_ = 0;
    unsigned consecutive_ = 0;
    bool valid_ = false;
    bool invalid_ = false;
};

} // namespace component
