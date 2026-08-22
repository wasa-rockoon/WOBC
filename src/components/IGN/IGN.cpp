#include "IGN.h"

namespace component {

// コンストラクタにピン番号を引数として追加
IGN::IGN(TwoWire& wire, int normal_pin, int high_pin, int low_pin, uint8_t unit_id, unsigned sample_freq_hz)
  : process::Component("IGN", component_id),
    wire_(wire),
    ina_IGN_(0x4D),
    normal_pin_(normal_pin),
    high_pin_(high_pin),
    low_pin_(low_pin),
    unit_id_(unit_id),
    // SampleTimer に LiPoPower の参照を渡す
    sample_timer_(*this, ina_IGN_, unit_id_, 1000 / sample_freq_hz) {
        
}

void IGN::setup() {
  start(sample_timer_);

  // ここでピンの設定をコンストラクタで受け取った引数を使用して行う
  digitalWrite(normal_pin_, LOW);
  digitalWrite(high_pin_, LOW);
  digitalWrite(low_pin_, LOW);
  pinMode(normal_pin_, OUTPUT);
  pinMode(high_pin_, OUTPUT);
  pinMode(low_pin_, OUTPUT);

  digitalWrite(normal_pin_, HIGH);
  active_pin_ = 0;
  last_pin_change_ms_ = millis();

  ina_IGN_.begin();
  ina_IGN_.setMaxCurrentShunt(4, 0.020);
}

void IGN::loop() {
  const unsigned long now = millis();
  if (now - last_pin_change_ms_ < pin_change_interval_ms) return;

  last_pin_change_ms_ = now;

  switch (active_pin_) {
    case 0:
      digitalWrite(normal_pin_, LOW);
      digitalWrite(high_pin_, HIGH);
      active_pin_ = 1;
      break;
    case 1:
      digitalWrite(high_pin_, LOW);
      digitalWrite(low_pin_, HIGH);
      active_pin_ = 2;
      break;
    default:
      digitalWrite(low_pin_, LOW);
      digitalWrite(normal_pin_, HIGH);
      active_pin_ = 0;
      break;
  }
}

// SampleTimer コンストラクタに LiPoPower の参照を追加
IGN::SampleTimer::SampleTimer(IGN& ign_ref, INA226& ina_ref, uint8_t unit_id_ref, unsigned interval_ms)
  : process::Timer("IGNTimer", interval_ms),
    ina_IGN_(ina_ref), ign_(ign_ref), unit_id_(unit_id_ref) {  // IGN への参照を保存
}

void IGN::SampleTimer::callback() {
  // INA226センサーのデータ取得
  int x1_mV = ina_IGN_.getBusVoltage() * 1000;
  int x1_mA = ina_IGN_.getCurrent() * 1000;
  int x1_mW = ina_IGN_.getPower() * 1000;

  // Powertelemetry_id パケット送信

  wcpp::Packet packet1 = newPacket(64);
  packet1.telemetry(Powertelemetry_id, ign_.component_id, unit_id_, 0xFF,
                    kernel::nextPacketSequence(unit_id_, 0xFF,
                                               ign_.component_id,
                                               wcpp::packet_type_mask | Powertelemetry_id));
  packet1.append("Vi").setInt(x1_mV);
  packet1.append("Ii").setInt(x1_mA);
  packet1.append("Pi").setInt(x1_mW);

  packet1.append("Ts").setInt(millis());

  sendPacket(packet1);

  // LiPotelemetry_id パケット送信
  /*wcpp::Packet packet2 = newPacket(64);
  packet2.telemetry(LiPotelemetry_id, lipo_power_.component_id);

  packet2.append("Vl").setInt(x1_mV);
  packet2.append("Il").setInt(x1_mA);
  packet2.append("Pl").setInt(x1_mW);

  sendPacket(packet2);*/
}

}
