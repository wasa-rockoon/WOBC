#include "IGN.h"

namespace component {

unsigned IGN::sampleIntervalMs(unsigned sample_freq_hz) {
  // 1〜1000 Hz以外の指定は、安全な既定値（1秒）へ丸める。
  return sample_freq_hz >= 1 && sample_freq_hz <= 1000
       ? 1000 / sample_freq_hz
       : 1000;
}

IGN::IGN(TwoWire& wire, int normal_pin, int high_pin, int low_pin,
         uint8_t unit_id, unsigned sample_freq_hz)
  : process::Component("IGN", component_id),
    ina_IGN_(0x4D, &wire),
    normal_pin_(normal_pin),
    high_pin_(high_pin),
    low_pin_(low_pin),
    unit_id_(unit_id),
    // 周期・ピン番号・ピンの重複をここで検証し、不正な構成では動作させない。
    config_valid_(sample_freq_hz >= 1 && sample_freq_hz <= 1000
               && normal_pin >= 0 && high_pin >= 0 && low_pin >= 0
               && normal_pin != no_pin && high_pin != no_pin && low_pin != no_pin
               && normal_pin != high_pin && normal_pin != low_pin
               && high_pin != low_pin),
    sample_timer_(ina_IGN_, unit_id_, sampleIntervalMs(sample_freq_hz)) {
}

const char* IGN::phaseName(Phase phase) {
  // テレメトリやログで読める名称へ状態列挙値を変換する。
  switch (phase) {
    case Phase::Startup:   return "Startup";
    case Phase::Countdown: return "Countdown";
    case Phase::Final:     return "Final";
    case Phase::Ignition:  return "Ignition";
    case Phase::Done:      return "Done";
    case Phase::Disarmed:  return "Disarmed";
    case Phase::Fault:     return "Fault";
  }
  return "Unknown";
}

bool IGN::prepareSafeOutputs() {
  if (!config_valid_) return false;

  // 方向を出力へ切り替える前にラッチをLOWにすることで、起動時に
  // 点火回路が一瞬でも通電されないようにする。
  digitalWrite(normal_pin_, LOW);
  digitalWrite(high_pin_, LOW);
  digitalWrite(low_pin_, LOW);
  pinMode(normal_pin_, OUTPUT);
  pinMode(high_pin_, OUTPUT);
  pinMode(low_pin_, OUTPUT);

  high_out_ = false;
  low_out_ = false;
  status_led_on_ = false;
  outputs_prepared_ = true;
  status_changed_ = true;
  return true;
}

bool IGN::begin(bool start_immediately) {
  if (!prepareSafeOutputs()) return false;

  // 電流計が使えない、または校正できない状態では点火を禁止する。
  const bool sensor_connected = ina_IGN_.begin();
  const int calibration_result = sensor_connected
                               ? ina_IGN_.setMaxCurrentShunt(4, 0.020)
                               : INA226_ERR_NORMALIZE_FAILED;
  if (!sensor_connected || calibration_result != INA226_ERR_NONE) {
    sequence_.fault(millis());
    forceSafeOutput();
    return false;
  }

  if (!startCutoffTask()) {
    sequence_.fault(millis());
    forceSafeOutput();
    return false;
  }

  // コンポーネントタスクの開始前に最初の遷移を予約する。これにより起動時に
  // 一時的なDisarmed状態を送信してしまうことを防ぐ。
  start_requested_ = start_immediately;
  begin_ok_ = true;
  if (!process::Component::begin()) {
    begin_ok_ = false;
    start_requested_ = false;
    forceSafeOutput();
    vTaskDelete(cutoff_task_handle_);
    cutoff_task_handle_ = nullptr;
  }
  return begin_ok_;
}

bool IGN::startSequence() {
  if (!begin_ok_ || sequence_.phase() != Phase::Disarmed
      || sequence_.hasStarted() || start_requested_) {
    return false;
  }
  start_requested_ = true;
  return true;
}

void IGN::abortSequence() {
  // まず監視タスクの遮断要求を無効化し、GPIOを安全側へ戻す。
  cutoff_armed_ = false;
  forceSafeOutput();
  abort_requested_ = true;
}

void IGN::setup() {
  // INA226の定期測定・テレメトリ送信を開始する。
  start(sample_timer_);
}

void IGN::loop() {
  const unsigned long now = millis();

  if (abort_requested_) {
    // 中止要求を状態機械に反映する。GPIOは要求受信時点で既に遮断済み。
    abort_requested_ = false;
    cutoff_armed_ = false;
    sequence_.abort(now);
  }

  if (cutoff_triggered_) {
    // 独立監視タスクが点火時間超過を検出した場合、完了状態へ遷移する。
    cutoff_triggered_ = false;
    sequence_.cutoff(now);
  }

  if (start_requested_) {
    // begin()またはstartSequence()で予約されたシーケンスを開始する。
    start_requested_ = false;
    if (!sequence_.start(now)) sequence_.fault(now);
  }

  IGNSequence::Snapshot snapshot = sequence_.update(now);

  if (snapshot.phase_changed && snapshot.phase == Phase::Ignition) {
    // 点火出力を有効にする前に、独立した時間超過監視を必ず作動させる。
    if (!armCutoff()) {
      sequence_.fault(now);
      snapshot = sequence_.update(now);
    }
  }

  applySnapshot(snapshot);

  if (snapshot.phase_changed) {
    LOG("IGN phase: %s", phaseName(snapshot.phase));
  }

  if (status_changed_ || millis() - last_status_ms_ >= status_interval_ms) {
    sendStatus(snapshot);
  }
}

void IGN::applySnapshot(const IGNSequence::Snapshot& snapshot) {
  // 新しい状態をログ・テレメトリへ出す前に、先にすべてのGPIOへ反映する。
  setOutput(snapshot.high, snapshot.low);
  setStatusLed(snapshot.status_led);
  if (snapshot.phase_changed) status_changed_ = true;
}

void IGN::setOutput(bool high, bool low) {
  portENTER_CRITICAL(&output_mux_);

  // 中止や監視タスクの遮断がコンポーネントタスクへ割り込んだ場合でも、
  // 古いスナップショットによる再通電を防ぐ。
  if (abort_requested_ || cutoff_triggered_) {
    high = false;
    low = false;
  } else if (low && !cutoff_armed_) {
    high = false;
    low = false;
  }

  // HIGHなしでLOWだけを有効にする状態は不正なので、異常な要求は
  // 非通電状態へ倒す。
  if (low && !high) {
    high = false;
    low = false;
  }

  // 遮断時は通電開始より先に出力を下げる。点火時はHIGHを先に確立し、
  // LOWを最後に有効にして回路を閉じる。
  if (!low && low_out_) {
    digitalWrite(low_pin_, LOW);
    low_out_ = false;
    status_changed_ = true;
  }
  if (!high && high_out_) {
    digitalWrite(high_pin_, LOW);
    high_out_ = false;
    status_changed_ = true;
  }
  if (high && !high_out_) {
    digitalWrite(high_pin_, HIGH);
    high_out_ = true;
    status_changed_ = true;
  }
  if (low && !low_out_) {
    digitalWrite(low_pin_, HIGH);
    low_out_ = true;
    status_changed_ = true;
  }

  portEXIT_CRITICAL(&output_mux_);
}

void IGN::forceSafeOutput() {
  if (!outputs_prepared_) {
    if (!prepareSafeOutputs()) return;
  }

  // 非常停止時はキャッシュした出力状態に頼らず、GPIOへ直接LOWを書き込む。
  portENTER_CRITICAL(&output_mux_);
  digitalWrite(low_pin_, LOW);
  digitalWrite(high_pin_, LOW);
  low_out_ = false;
  high_out_ = false;
  status_changed_ = true;
  portEXIT_CRITICAL(&output_mux_);
}

void IGN::setStatusLed(bool on) {
  // 状態が変化したときだけ書き込み、不要なGPIO操作を避ける。
  if (on == status_led_on_) return;
  digitalWrite(normal_pin_, on ? HIGH : LOW);
  status_led_on_ = on;
  status_changed_ = true;
}

void IGN::sendStatus(const IGNSequence::Snapshot& snapshot) {
  // 現在の段階、経過時間、GPIO出力、安全状態を1パケットにまとめて送る。
  wcpp::Packet packet = newPacket(80);
  packet.telemetry(Statustelemetry_id, component_id, unit_id_, 0xFF,
                   kernel::nextPacketSequence(unit_id_, 0xFF, component_id,
                                              wcpp::packet_type_mask
                                              | Statustelemetry_id));
  packet.append("Ph").setEnum(snapshot.phase);
  packet.append("Et").setInt((int)snapshot.phase_elapsed_ms);
  packet.append("St").setInt((int)snapshot.sequence_elapsed_ms);
  packet.append("Rt").setInt((int)snapshot.remaining_ms);
  packet.append("Bz").setBool(high_out_ && !low_out_);
  packet.append("Ig").setBool(high_out_ && low_out_);
  packet.append("Hi").setBool(high_out_);
  packet.append("Lo").setBool(low_out_);
  packet.append("Nl").setBool(status_led_on_);
  packet.append("Ok").setBool(begin_ok_ && snapshot.phase != Phase::Fault);
  packet.append("Ts").setInt((int)millis());
  sendPacket(packet);

  last_status_ms_ = millis();
  status_changed_ = false;
}

bool IGN::startCutoffTask() {
  // 既に作成済みなら再利用し、なければ高優先度の監視タスクを起動する。
  if (cutoff_task_handle_ != nullptr) return true;
  return xTaskCreate(cutoffTaskEntry, "IGNCutoff", cutoff_task_stack_size,
                     this, configMAX_PRIORITIES - 1,
                     &cutoff_task_handle_) == pdPASS;
}

bool IGN::armCutoff() {
  // 点火開始を監視タスクへ通知し、最大点火時間の計測を開始する。
  if (cutoff_task_handle_ == nullptr) return false;
  cutoff_armed_ = true;
  if (xTaskNotify(cutoff_task_handle_, 1, eSetValueWithOverwrite) == pdPASS) {
    return true;
  }
  cutoff_armed_ = false;
  return false;
}

void IGN::cutoffTaskEntry(void* instance) {
  IGN* ign = static_cast<IGN*>(instance);
  for (;;) {
    // 点火開始通知を待ち、通知後に許容点火時間だけ待機する。
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(IGNSequence::ignition_ms));
    if (ign->cutoff_armed_) ign->cutoffFromWatchdog();
  }
}

void IGN::cutoffFromWatchdog() {
  // 点火時間上限を超えたため、状態機械の処理を待たずに出力を強制遮断する。
  cutoff_armed_ = false;
  cutoff_triggered_ = true;
  forceSafeOutput();
}

IGN::SampleTimer::SampleTimer(INA226& ina_ref, uint8_t unit_id_ref,
                              unsigned interval_ms)
  : process::Timer("IGNTimer", interval_ms),
    ina_IGN_(ina_ref),
    unit_id_(unit_id_ref) {
}

void IGN::SampleTimer::callback() {
  // INA226の基本単位（V/A/W）をテレメトリ用のmV/mA/mWへ変換する。
  const int voltage_mV = ina_IGN_.getBusVoltage() * 1000;
  const int current_mA = ina_IGN_.getCurrent() * 1000;
  const int power_mW = ina_IGN_.getPower() * 1000;

  wcpp::Packet packet = newPacket(64);
  packet.telemetry(Powertelemetry_id, IGN::component_id, unit_id_, 0xFF,
                   kernel::nextPacketSequence(unit_id_, 0xFF,
                                              IGN::component_id,
                                              wcpp::packet_type_mask
                                              | Powertelemetry_id));
  packet.append("Vi").setInt(voltage_mV);
  packet.append("Ii").setInt(current_mA);
  packet.append("Pi").setInt(power_mW);
  packet.append("Ts").setInt(millis());
  sendPacket(packet);
}

}  // namespace component
