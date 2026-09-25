#include "src/components/Heater/HeaterControl.h"
#include "src/library/wcpp/cpp/packet.h"
#include <cassert>
#include <cstdio>
#include <limits>

using component::HeaterControl;
using State = HeaterControl::State;

int main() {
    HeaterControl control;
    assert(control.evaluate(0, 8000) == State::Stale);
    control.observe(20, 0);
    assert(control.evaluate(0, 8000) == State::Recovering);
    control.observe(20, 500);
    control.missing();
    control.observe(20, 1000);
    control.observe(20, 1500);
    assert(control.evaluate(1500, 8000) == State::Recovering);
    control.observe(20, 2000);
    assert(control.evaluate(2000, 8000) == State::On);

    // Identical temperatures are fresh measurements; the exact deadline is off.
    control.observe(20, 2500);
    control.missing();
    assert(control.evaluate(4499, 8000) == State::On);
    assert(control.evaluate(4500, 8000) == State::Stale);
    control.observe(20, 4501);
    assert(control.evaluate(4501, 8000) == State::Recovering);
    control.observe(20, 5001);
    control.observe(20, 5501);
    assert(control.evaluate(5501, 8000) == State::On);
    assert(control.evaluate(5501, 6399) == State::LowBattery);
    assert(control.evaluate(5501, 6400) == State::On);
    control.observe(40, 6000);
    assert(control.evaluate(6000, 8000) == State::Off);
    control.observe(39.9f, 6500);
    assert(control.evaluate(6500, 8000) == State::On);

    // An explicitly invalid CH1 sample stops immediately, without waiting 2 s.
    control.invalidate();
    assert(control.evaluate(6501, 8000) == State::Invalid);
    control.observe(20, 7000);
    control.observe(20, 7500);
    assert(control.evaluate(7500, 8000) == State::Recovering);
    control.observe(20, 8000);
    assert(control.evaluate(8000, 8000) == State::On);
    control.observe(std::numeric_limits<float>::quiet_NaN(), 8500);
    assert(control.evaluate(8500, 8000) == State::Invalid);
    control.observe(std::numeric_limits<float>::infinity(), 9000);
    assert(control.evaluate(9000, 8000) == State::Invalid);

    // A late measurement cannot bypass recovery even if no evaluation ran.
    HeaterControl late;
    late.observe(20, 0);
    late.observe(20, 500);
    late.observe(20, 1000);
    late.observe(20, 3000);
    assert(late.evaluate(3000, 8000) == State::Recovering);

    // millis() rollover preserves unsigned elapsed-time comparisons.
    HeaterControl rollover;
    const uint32_t start = UINT32_MAX - 1000;
    rollover.observe(20, start);
    rollover.observe(20, start + 500);
    rollover.observe(20, start + 1000);
    assert(rollover.evaluate(1998, 8000) == State::On);
    assert(rollover.evaluate(1999, 8000) == State::Stale);

    // Long fault strings must not truncate Hs or the trailing timestamp.
    for (State state : {State::On, State::Off, State::LowBattery,
                        State::Stale, State::Invalid, State::Recovering}) {
        uint8_t buffer[96] = {};
        auto packet = wcpp::Packet::empty(buffer, sizeof(buffer));
        packet.telemetry('H', 0x46, 0x62, 0xFF, 1);
        assert(packet.append("Ca").setFloat16(20));
        assert(packet.append("Cb").setFloat16(100));
        assert(packet.append("Cc").setFloat16(-100));
        for (const char* key : {"Vc", "Vb", "Ib", "Pb"}) {
            assert(packet.append(key).setInt(INT32_MAX));
        }
        assert(packet.append("Hs").setString(HeaterControl::status(state)));
        assert(packet.append("Ts").setInt(INT32_MAX));
    }
    std::puts("Heater CH1 control tests passed");
}
