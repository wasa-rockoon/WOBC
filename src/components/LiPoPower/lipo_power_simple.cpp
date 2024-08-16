#include "lipo_power_simple.h"

namespace component {

LiPoPowerSimple::LiPoPowerSimple(TwoWire& wire, unsigned sample_freq_hz /*TODO ピン設定*/)
  : process::Component("LiPoPowerSimple", component_id),
    wire_(wire),
    sample_timer_("LiPoPowerSimpleTimer", 1000 / sample_freq_hz) {
}

void LiPoPowerSimple::setup() {
  start(sample_timer_);
}

void LiPoPowerSimple::SampleTimer::callback() { // Timerで定期的に実行される関数

  int x_mV, x_mA;
  bool charging;
  // TODO INA読み取り

  wcpp::Packet packet = newPacket(64);
  packet.telemetry(telemetry_id, component_id());
  packet.append("Vx").setInt(x_mV);
  packet.append("Ix").setInt(x_mA);
  packet.append("Ch").setBool(charging);
  // ... TODO

  sendPacket(packet);
}


}
