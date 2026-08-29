#pragma once

#include <library/wobc.h>
#include <Wire.h>
#include <components/LiPoPower/INA226.h>
#include "IGNSequence.h"

namespace component {

class IGN : public process::Component {
public:
  static const uint8_t component_id = 36;
  static const uint8_t Powertelemetry_id = 'I';   // 電圧・電流・電力
  static const uint8_t Statustelemetry_id = 'S';  // 点火シーケンスの状態

  using Phase = IGNSequence::Phase;

  IGN(TwoWire& wire, int normal_pin, int high_pin, int low_pin, uint8_t unit_id,
      unsigned sample_freq_hz = 1);

  // 点火系GPIOを同期的に安全状態へ移す。kernel初期化より先に呼び出せる。
  bool prepareSafeOutputs();

  // INA226とタスクを初期化する。start_immediately=trueの場合は最初の
  // IGNループからフェーズ0を開始する。
  bool begin(bool start_immediately = false);

  // 初期化後に一度だけ開始できる。同一boot中の再点火は拒否する。
  bool startSequence();

  // どの状態からでも点火出力を直ちにLOWへ戻す。
  void abortSequence();

  Phase phase() const { return sequence_.phase(); }
  bool healthy() const { return begin_ok_; }
  static const char* phaseName(Phase phase);

protected:
  INA226 ina_IGN_;

  int normal_pin_;
  int high_pin_;
  int low_pin_;
  uint8_t unit_id_;
  bool config_valid_;

  IGNSequence sequence_;

  volatile bool high_out_ = false;
  volatile bool low_out_ = false;
  volatile bool status_led_on_ = false;
  volatile bool status_changed_ = true;
  volatile bool start_requested_ = false;
  volatile bool abort_requested_ = false;
  volatile bool cutoff_armed_ = false;
  volatile bool cutoff_triggered_ = false;

  bool outputs_prepared_ = false;
  bool begin_ok_ = false;
  unsigned long last_status_ms_ = 0;

  static constexpr unsigned long status_interval_ms = 1000;
  static constexpr unsigned cutoff_task_stack_size = 2048;

  TaskHandle_t cutoff_task_handle_ = nullptr;
  portMUX_TYPE output_mux_ = portMUX_INITIALIZER_UNLOCKED;

  void setup() override;
  void loop() override;

  void applySnapshot(const IGNSequence::Snapshot& snapshot);
  void setOutput(bool high, bool low);
  void forceSafeOutput();
  void setStatusLed(bool on);
  void sendStatus(const IGNSequence::Snapshot& snapshot);

  bool startCutoffTask();
  bool armCutoff();
  void cutoffFromWatchdog();
  static void cutoffTaskEntry(void* instance);
  static unsigned sampleIntervalMs(unsigned sample_freq_hz);

  class SampleTimer : public process::Timer {
  public:
    SampleTimer(INA226& ina_ref, uint8_t unit_id_ref, unsigned interval_ms);

  protected:
    void callback() override;

  private:
    INA226& ina_IGN_;
    uint8_t unit_id_;
  } sample_timer_;
};

}  // namespace component
