#include <library/wobc.h>
#include <Wire.h>
#include <components/LiPoPower/INA226.h>

namespace component {

class IGN : public process::Component {
public:
  static const uint8_t component_id = 'I';
  static const uint8_t Powertelemetry_id = 'I';
  static const uint8_t LiPotelemetry_id = 'L';
  static const uint8_t Heatertelemetry_id = 'H';

  // コンストラクタにピン指定用の引数を追加
  IGN(TwoWire& wire, int normal_pin, int high_pin, int low_pin, uint8_t unit_id, unsigned sample_freq_hz = 1);

protected:
  TwoWire& wire_;
  INA226 ina_IGN_;

  // ピン番号を保持するメンバ変数
  int normal_pin_;
  int high_pin_;
  int low_pin_;
  uint8_t unit_id_;
  uint8_t active_pin_ = 0;
  unsigned long last_pin_change_ms_ = 0;

  static constexpr unsigned pin_change_interval_ms = 1000;


  void setup() override;
  void loop() override;

  // SampleTimer に LiPoPower の参照を渡すためのクラス
  class SampleTimer : public process::Timer {
  public:
    // LiPoPower への参照を追加
    SampleTimer(IGN& ign_ref, INA226& ina1_ref, uint8_t unit_id_ref, unsigned interval_ms);

  protected:
    void callback() override;

  private:
    INA226& ina_IGN_;
    IGN& ign_;  // IGN の参照を保持
    uint8_t unit_id_;
  } sample_timer_;
};

}
