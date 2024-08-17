#include "pressure.h"

namespace component {

Pressure::Pressure(TwoWire& wire, unsigned sample_freq_hz)
  : process::Component("Pressure", component_id),
    wire_(wire),
    sample_timer_("PressureTimer", 1000 / sample_freq_hz) {
}

void Pressure::setup() {
  start(sample_timer_);
  storeOnCommand('Q'); // 高度規正値の設定コマンド
}

void Pressure::SampleTimer::callback() { // Timerで定期的に実行される関数

  // 高度規正値を不揮発メモリから読み込み
  float sealevel_Pa = 1013.0;
  wcpp::Packet qnh = loadPacket('Q'); 
  if (qnh) {
    auto e = qnh.find("Sp");
    if (e) sealevel_Pa = (*e).getFloat32();
  }

  // TODO 気圧センサ読み取り

  wcpp::Packet packet = newPacket(64);
  packet.telemetry(telemetry_id, component_id());
  packet.append("Sp").setFloat32(sealevel_Pa);
  // ... TODO

  sendPacket(packet);
}


}
