#include "pressure.h"

namespace component {

Pressure::Pressure(TwoWire& wire, unsigned sample_freq_hz)
  : process::Component("Pressure", component_id),
    wire_(wire),
    sample_timer_(*this, bme, 1000 / sample_freq_hz) {
}

void Pressure::setup() {
  start(sample_timer_);
  storeOnCommand('Q'); // 高度規正値の設定コマンド
  Wire.begin();
  while(!bme.begin()){
    LOG("Could not find BME280");
    delay(1000);
  }
}

Pressure::SampleTimer::SampleTimer(Pressure& pressure_ref, BME280I2C& bme_ref, unsigned interval_ms)
  : process::Timer("Pressure", interval_ms),
    bme_(bme_ref), pressure_(pressure_ref) { 
}

void Pressure::SampleTimer::callback() { // Timerで定期的に実行される関数

  // 高度規正値を不揮発メモリから読み込み
  double sealevel_Pa = 1013.0;
  wcpp::Packet qnh = loadPacket('Q'); 
  if (qnh) {
    auto e = qnh.find("Sp");
    if (e) sealevel_Pa = (*e).getFloat32();
  }

  float temp(NAN), hum(NAN), pres(NAN);
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
  bme_.read(pres, temp, hum, tempUnit, presUnit);
  double pressureAlt = (pow((sealevel_Pa/pres),(1.0/5.257))-1.0)*(temp+273.15)/0.0065;

  wcpp::Packet packet = newPacket(64);
  packet.telemetry(telemetry_id, component_id());
  packet.append("Sp").setInt((int)sealevel_Pa);
  packet.append("PR").setInt((int)pres);
  packet.append("TE").setInt((int)temp);
  packet.append("HU").setInt((int)hum);
  packet.append("PA").setInt((int)pressureAlt);
  // ... TODO
  sendPacket(packet);
}


}
