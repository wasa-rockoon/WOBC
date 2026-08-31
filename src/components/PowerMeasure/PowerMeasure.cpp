#include "PowerMeasure.h"

namespace component {
    PowerMeasure::PowerMeasure(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz)
        : process::Component("PowerMeasure", component_id),
          wire_(wire),
          ina_power(0x47, &wire_),
          ina_servo(0x45, &wire_),
          unit_id_(unit_id),
          sample_timer_(*this, ina_power, ina_servo, unit_id_, 1000 /  sample_freq_hz) {
    }

    void PowerMeasure::setup() {
        start(sample_timer_);

        ina_power.begin();
        ina_power.setMaxCurrentShunt(20, 0.003);
        ina_servo.begin();
        ina_servo.setMaxCurrentShunt(3, 0.020);
    }

    PowerMeasure::SampleTimer::SampleTimer(PowerMeasure& power_measure_ref, INA226& ina_power_ref, INA226& ina_servo_ref, uint8_t unit_id_ref, unsigned interval_ms)
        : process::Timer("PowerMeasureTimer", interval_ms),
          ina_power_(ina_power_ref), ina_servo_(ina_servo_ref), power_measure_(power_measure_ref), unit_id_(unit_id_ref) {
    }

    void PowerMeasure::SampleTimer::callback() {
        int power_mV = ina_power_.getBusVoltage() * 1000;
        int power_mA = ina_power_.getCurrent() * 1000;
        int power_mW = ina_power_.getPower() * 1000;
        int servo_mV = ina_servo_.getBusVoltage() * 1000;
        int servo_mA = ina_servo_.getCurrent() * 1000;
        int servo_mW = ina_servo_.getPower() * 1000;

        wcpp::Packet packet = power_measure_.newPacket(64);
        packet.telemetry(telemetry_id, component_id(), unit_id_, 0xFF, 1234);
        packet.append("Pv").setInt(power_mV); // 3セルLiPoの電圧
        packet.append("Pi").setInt(power_mA); // 3セルLiPoの電流
        packet.append("Pp").setInt(power_mW); // 3セルLiPoの電力
        packet.append("Sv").setInt(servo_mV); // サーボ用DCDCの電圧
        packet.append("Si").setInt(servo_mA); // サーボ用DCDCの電流
        packet.append("Sp").setInt(servo_mW); // サーボ用DCDCの電力
        power_measure_.sendPacket(packet);
    }
}
