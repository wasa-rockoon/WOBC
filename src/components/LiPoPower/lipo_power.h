#include <library/wobc.h>
#include <Wire.h>
#include "INA226.h"

namespace component {

class LiPoPower : public process::Component {
public:
  static const uint8_t component_id = 0x01;
  static const uint8_t Powertelemetry_id = 's';
  static const uint8_t LiPotelemetry_id = 'L';
  static const uint8_t Heatertelemetry_id = 'H';

  // コンストラクタにピン指定用の引数を追加
  LiPoPower(TwoWire& wire, int st_pin, int pg_pin, int stat1_pin, int stat2_pin, int heat_pin, int charge_led_pin, int temp_pin, unsigned sample_freq_hz = 10);

protected:
  TwoWire& wire_;
  INA226 ina1;
  INA226 ina2;
  INA226 ina3;

  // ピン番号を保持するメンバ変数
  int st_pin_;
  int pg_pin_;
  int stat1_pin_;
  int stat2_pin_;
  int heat_pin_;
  int charge_led_pin_;
  int temp_pin_;

  void setup() override;

  // SampleTimer に LiPoPower の参照を渡すためのクラス
  class SampleTimer : public process::Timer {
  public:
    // LiPoPower への参照を追加
    SampleTimer(LiPoPower& lipo_power_ref, INA226& ina1_ref, INA226& ina2_ref, INA226& ina3_ref, unsigned interval_ms);
    
  protected:
    void callback() override;

  private:
    INA226& ina1_;
    INA226& ina2_;
    INA226& ina3_;
    LiPoPower& lipo_power_;  // LiPoPower の参照を保持
  } sample_timer_;
};

}
