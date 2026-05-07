#include "lipo_power_simple.h"

namespace component {

LiPoPowerSimple::LiPoPowerSimple(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz)
  : process::Component("LiPoPowerSimple", component_id),
    wire_(wire),
    ina(0x4F),
    unit_id_(unit_id),

    sample_timer_(*this, ina, unit_id, 1000 / sample_freq_hz) {
}

void LiPoPowerSimple::setup() {
  start(sample_timer_);

  ina.begin();
  ina.setMaxCurrentShunt(1, 0.05);
}

LiPoPowerSimple::SampleTimer::SampleTimer(LiPoPowerSimple& lipo_power_ref, INA226& ina_ref, uint8_t unit_id_ref, unsigned interval_ms)
  : process::Timer("LiPoPowerSimpleTimer", interval_ms),
    ina_(ina_ref), unit_id_(unit_id_ref), lipo_power_simple_(lipo_power_ref) {
}

void LiPoPowerSimple::SampleTimer::callback() { // Timerで定期的に実行される関数
  // TODO INA読み取り
  int x_mV = ina_.getBusVoltage() * 1000; // INA226から電圧を読み取る（例）
  int x_mA = ina_.getCurrent() * 1000;    // INA226から電流を読み取る（例）
  int x_mW = ina_.getPower() * 1000;      // INA226から電力を読み取る（例）
  wcpp::Packet packet = newPacket(64);
  packet.telemetry(telemetry_id, component_id());
  packet.append("Vx").setInt(x_mV);
  packet.append("Ix").setInt(x_mA);
  packet.append("Px").setInt(x_mW);
  //packet.append("Ch").setBool(charging);
  // ... TODO

  sendPacket(packet);
}


}
