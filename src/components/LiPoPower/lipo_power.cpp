#include "lipo_power.h"

namespace component {

// コンストラクタにピン番号を引数として追加
LiPoPower::LiPoPower(TwoWire& wire, int st_pin, int pg_pin, int stat1_pin, int stat2_pin, int heat_pin, int charge_led_pin, int temp_pin, uint8_t unit_id, unsigned sample_freq_hz)
  : process::Component("LiPoPower", component_id),
    wire_(wire),
    ina1(0x4F),
    ina2(0x4D),
    ina3(0x4E),
    st_pin_(st_pin),              // ピン番号をメンバ変数に保存
    pg_pin_(pg_pin),
    stat1_pin_(stat1_pin),
    stat2_pin_(stat2_pin),
    heat_pin_(heat_pin),
    charge_led_pin_(charge_led_pin),
    temp_pin_(temp_pin),
    unit_id_(unit_id),
    // SampleTimer に LiPoPower の参照を渡す
    sample_timer_(*this, ina1, ina2, ina3, unit_id, 1000 / sample_freq_hz) {
}

void LiPoPower::setup() {
  start(sample_timer_);

  // ここでピンの設定をコンストラクタで受け取った引数を使用して行う
  pinMode(st_pin_, INPUT);
  pinMode(pg_pin_, INPUT_PULLUP);
  pinMode(stat1_pin_, INPUT_PULLUP);
  pinMode(stat2_pin_, INPUT_PULLUP);
  pinMode(charge_led_pin_, OUTPUT);

  ina1.begin();
  ina1.setMaxCurrentShunt(1, 0.05);
  ina2.begin();
  ina2.setMaxCurrentShunt(1, 0.05);
  ina3.begin();
  ina3.setMaxCurrentShunt(1, 0.05);
}

// SampleTimer コンストラクタに LiPoPower の参照を追加
LiPoPower::SampleTimer::SampleTimer(LiPoPower& lipo_power_ref, INA226& ina1_ref, INA226& ina2_ref, INA226& ina3_ref, uint8_t unit_id_ref, unsigned interval_ms)
  : process::Timer("LiPoPowerTimer", interval_ms),
    ina1_(ina1_ref), ina2_(ina2_ref), ina3_(ina3_ref), unit_id_(unit_id_ref), lipo_power_(lipo_power_ref) {  // LiPoPower への参照を保存
}

void LiPoPower::SampleTimer::callback() {
  // INA226センサーのデータ取得
  int x1_mV = ina1_.getBusVoltage() * 1000;
  int x1_mA = ina1_.getCurrent() * 1000;
  int x1_mW = ina1_.getPower() * 1000;
  int x2_mV = ina2_.getBusVoltage() * 1000;
  int x2_mA = ina2_.getCurrent() * 1000;
  int x2_mW = ina2_.getPower() * 1000;
  int x3_mV = ina3_.getBusVoltage() * 1000;
  int x3_mA = ina3_.getCurrent() * 1000;
  int x3_mW = ina3_.getPower() * 1000;

  // LiPoPower クラスのピン番号を使ってデジタル入力を読み込む
  bool source = digitalRead(lipo_power_.st_pin_) ? 0 : 1;
  bool charge = 0;

  if (!digitalRead(lipo_power_.pg_pin_) && !digitalRead(lipo_power_.stat1_pin_) && digitalRead(lipo_power_.stat2_pin_)) {
    charge = 1;
  }

  // LED の状態を更新
  digitalWrite(lipo_power_.charge_led_pin_, charge);

  // Powertelemetry_id パケット送信

  wcpp::Packet packet1 = newPacket(64);
  packet1.telemetry(Powertelemetry_id, lipo_power_.component_id, unit_id_, 0xFF, 1234);
  packet1.append("Sc").setBool(source);
  packet1.append("Vp").setInt(x1_mV);
  packet1.append("Ip").setInt(x1_mA);
  packet1.append("Vb").setInt(x2_mV);
  packet1.append("Pp").setInt(x1_mW);
  packet1.append("Vd").setInt(x3_mV);
  packet1.append("Id").setInt(x3_mA);
  packet1.append("Pd").setInt(x3_mW);
  
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
