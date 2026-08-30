#pragma once

#include <library/wobc.h>
#include <Wire.h>
#include <components/LiPoPower/INA226.h>
#include "IGNSequence.h"

namespace component {

// 点火回路を制御し、電力・状態テレメトリを送信するコンポーネント。
class IGN : public process::Component {
public:
  static const uint8_t component_id = 36;
  // INA226で計測した電圧・電流・電力を送信するテレメトリ種別。
  static const uint8_t Powertelemetry_id = 'I';
  // 点火シーケンスの段階とGPIOの状態を送信するテレメトリ種別。
  static const uint8_t Statustelemetry_id = 'S';

  using Phase = IGNSequence::Phase;

  IGN(TwoWire& wire, int normal_pin, int high_pin, int low_pin, uint8_t unit_id,
      unsigned sample_freq_hz = 1);

  // 点火用GPIOをすべてLOW出力に初期化する。カーネルやタスクの起動前にも
  // 呼び出せるため、起動失敗時でも点火回路を非通電に保てる。
  bool prepareSafeOutputs();

  // INA226と非常停止用タスクを初期化してコンポーネントを開始する。
  // start_immediately が true の場合は、開始直後に点火シーケンスを予約する。
  bool begin(bool start_immediately = false);

  // 初期化後に一度だけ点火シーケンスの開始を予約する。同一ブート中の
  // 再点火は許可しない。
  bool startSequence();

  // どの段階からでも点火出力を直ちにLOWへ戻し、シーケンスを中止する。
  void abortSequence();

  Phase phase() const { return sequence_.phase(); }
  bool healthy() const { return begin_ok_; }
  static const char* phaseName(Phase phase);

protected:
  // 点火回路の電圧・電流・電力を測定するINA226。
  INA226 ina_IGN_;

  int normal_pin_;
  int high_pin_;
  int low_pin_;
  uint8_t unit_id_;
  bool config_valid_;

  // 時間に応じた点火出力の要求を生成する、ハードウェア非依存の状態機械。
  IGNSequence sequence_;

  // GPIOに最後に出力した状態と、ステータスLEDの状態。
  volatile bool high_out_ = false;
  volatile bool low_out_ = false;
  volatile bool status_led_on_ = false;
  volatile bool status_changed_ = true;
  // コンポーネントタスクと非常停止タスクの間で受け渡す要求・状態。
  volatile bool start_requested_ = false;
  volatile bool abort_requested_ = false;
  volatile bool cutoff_armed_ = false;
  volatile bool cutoff_triggered_ = false;

  // GPIO初期化およびコンポーネント初期化が完了したかを示す。
  bool outputs_prepared_ = false;
  bool begin_ok_ = false;
  unsigned long last_status_ms_ = 0;

  // 状態が変わらない場合でも、この周期で状態テレメトリを送信する。
  static constexpr unsigned long status_interval_ms = 1000;
  static constexpr unsigned cutoff_task_stack_size = 2048;

  // 点火時間の上限を独立して監視するFreeRTOSタスク。
  TaskHandle_t cutoff_task_handle_ = nullptr;
  portMUX_TYPE output_mux_ = portMUX_INITIALIZER_UNLOCKED;

  void setup() override;
  void loop() override;

  // 状態機械の出力をGPIOとステータスLEDへ反映する。
  void applySnapshot(const IGNSequence::Snapshot& snapshot);
  void setOutput(bool high, bool low);
  void forceSafeOutput();
  void setStatusLed(bool on);
  void sendStatus(const IGNSequence::Snapshot& snapshot);

  // 点火時間超過時に強制遮断する監視タスクを作成・制御する。
  bool startCutoffTask();
  bool armCutoff();
  void cutoffFromWatchdog();
  static void cutoffTaskEntry(void* instance);
  static unsigned sampleIntervalMs(unsigned sample_freq_hz);

  // INA226の測定値を定期的に電力テレメトリとして送信するタイマー。
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
