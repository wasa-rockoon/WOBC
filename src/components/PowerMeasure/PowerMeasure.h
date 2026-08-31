#include <library/wobc.h>
#include <Wire.h>
#include "INA226.h"

namespace component {

class PowerMeasure : public process::Component {
public:
    static const uint8_t component_id = 0x49;
    static const uint8_t telemetry_id = 'P';

    PowerMeasure(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz);

protected:
    TwoWire& wire_;
    INA226 ina_power;
    INA226 ina_servo;

    uint8_t unit_id_;

void setup() override;

class SampleTimer : public process::Timer {
public:
    SampleTimer(PowerMeasure& power_measure_ref, INA226& ina_power_ref, INA226& ina_servo_ref, uint8_t unit_id_ref, unsigned interval_ms);

protected:
    void callback() override;

private:
    INA226& ina_power_;
    INA226& ina_servo_;
    PowerMeasure& power_measure_;
    uint8_t unit_id_;
} sample_timer_;
};

} // namespace component
