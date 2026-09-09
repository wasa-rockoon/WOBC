#include "Nichrome.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <soc/gpio_struct.h>
#define Nichrome_ISR_ATTR IRAM_ATTR
#else
#define Nichrome_ISR_ATTR
#endif

namespace component {

unsigned Nichrome::sampleIntervalMs(unsigned sample_freq_hz) {
  // 1〜1000 Hz以外の指定は、安全な既定値（1秒）へ丸める。
  return sample_freq_hz >= 1 && sample_freq_hz <= 1000
       ? 1000 / sample_freq_hz
       : 1000;
}

Nichrome::Nichrome(TwoWire& wire, int normal_pin, int high_pin, int low_pin,
         uint8_t unit_id, unsigned sample_freq_hz)
  : process::Component("Nichrome", component_id),
    ina_Nichrome_(0x4D, &wire),
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
    sample_timer_(ina_Nichrome_, unit_id_, sampleIntervalMs(sample_freq_hz)) {
}

const char* Nichrome::phaseName(Phase phase) {
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

bool Nichrome::prepareSafeOutputs() {
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

bool Nichrome::begin(bool start_immediately) {
  if (!prepareSafeOutputs()) return false;

  // 電流計が使えない、または校正できない状態では点火を禁止する。
  const bool sensor_connected = ina_Nichrome_.begin();
  const int calibration_result = sensor_connected
                               ? ina_Nichrome_.setMaxCurrentShunt(4, 0.020)
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

bool Nichrome::startSequence() {
  if (!begin_ok_ || sequence_.phase() != Phase::Disarmed
      || sequence_.hasStarted() || start_requested_) {
    return false;
  }

  portENTER_CRITICAL(&output_mux_);
  start_requested_ = true;
  flight_pin_abort_armed_ = true;
  portEXIT_CRITICAL(&output_mux_);
  return true;
}

void Nichrome::abortSequence() {
  // 先に中止要求を立てて開始予約と監視を無効化する。これにより、出力停止と
  // 状態機械への反映の間に古い開始要求やSnapshotが出力を再有効化できない。
  portENTER_CRITICAL(&output_mux_);
  start_requested_ = false;
  abort_requested_ = true;
  cutoff_armed_ = false;
  flight_pin_abort_armed_ = false;
  portEXIT_CRITICAL(&output_mux_);

  forceSafeOutput();
}

void Nichrome_ISR_ATTR Nichrome::abortSequenceFromISR() {
  portENTER_CRITICAL_ISR(&output_mux_);

  // 開始要求前およびDone/Fault/Disarmed移行後の挿入割り込みは無視する。
  if (!flight_pin_abort_armed_) {
    portEXIT_CRITICAL_ISR(&output_mux_);
    return;
  }

  // ISRではブロッキング処理、ログ、パケット送信を行わない。LOW側を先に落とし、
  // 続いてHIGH側を落として点火回路を直ちに開放する。
  start_requested_ = false;
  abort_requested_ = true;
  cutoff_armed_ = false;
  flight_pin_abort_armed_ = false;
#if defined(ARDUINO_ARCH_ESP32)
  // ArduinoのdigitalWrite経由ではなくGPIOのwrite-one-to-clearレジスタを使い、
  // フラッシュキャッシュ停止中でもISRから遮断できるようにする。
  if (low_pin_ < 32) {
    GPIO.out_w1tc = (1UL << low_pin_);
  } else {
    GPIO.out1_w1tc.val = (1UL << (low_pin_ - 32));
  }
  if (high_pin_ < 32) {
    GPIO.out_w1tc = (1UL << high_pin_);
  } else {
    GPIO.out1_w1tc.val = (1UL << (high_pin_ - 32));
  }
#else
  digitalWrite(low_pin_, LOW);
  digitalWrite(high_pin_, LOW);
#endif
  low_out_ = false;
  high_out_ = false;
  status_changed_ = true;

  portEXIT_CRITICAL_ISR(&output_mux_);
}

void Nichrome::setup() {
  // INA226の定期測定・テレメトリ送信を開始する。
  start(sample_timer_);
}

void Nichrome::loop() {
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
    if (sequence_.start(now)) {
      sample_timer_.changePeriod(ignition_sample_interval_ms);
    } else {
      sequence_.fault(now);
    }
  }

  NichromeSequence::Snapshot snapshot = sequence_.update(now);

  // 開始要求から活動中フェーズまでだけFlightPin割り込みによる中止を許可する。
  // ISRが既に中止を要求している場合は、状態機械へ反映される前でも再アームしない。
  const bool sequence_active = snapshot.phase == Phase::Startup
                            || snapshot.phase == Phase::Countdown
                            || snapshot.phase == Phase::Final
                            || snapshot.phase == Phase::Ignition;
  portENTER_CRITICAL(&output_mux_);
  flight_pin_abort_armed_ = sequence_active && !abort_requested_;
  portEXIT_CRITICAL(&output_mux_);

  if (snapshot.phase_changed && snapshot.phase == Phase::Ignition) {
    // 点火出力を有効にする前に、独立した時間超過監視を必ず作動させる。
    if (!armCutoff()) {
      sequence_.fault(now);
      snapshot = sequence_.update(now);
    }
  }

  applySnapshot(snapshot);

  if (snapshot.phase_changed) {
    LOG("Nichrome phase: %s", phaseName(snapshot.phase));
  }

  if (status_changed_ || millis() - last_status_ms_ >= status_interval_ms) {
    sendStatus(snapshot);
  }
}

void Nichrome::applySnapshot(const NichromeSequence::Snapshot& snapshot) {
  // 新しい状態をログ・テレメトリへ出す前に、先にすべてのGPIOへ反映する。
  setOutput(snapshot.high, snapshot.low);
  setStatusLed(snapshot.status_led);
  if (snapshot.phase_changed) status_changed_ = true;
}

void Nichrome::setOutput(bool high, bool low) {
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

void Nichrome::forceSafeOutput() {
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

void Nichrome::setStatusLed(bool on) {
  // 状態が変化したときだけ書き込み、不要なGPIO操作を避ける。
  if (on == status_led_on_) return;
  digitalWrite(normal_pin_, on ? HIGH : LOW);
  status_led_on_ = on;
  status_changed_ = true;
}

void Nichrome::sendStatus(const NichromeSequence::Snapshot& snapshot) {
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

bool Nichrome::startCutoffTask() {
  // 既に作成済みなら再利用し、なければ高優先度の監視タスクを起動する。
  if (cutoff_task_handle_ != nullptr) return true;
  return xTaskCreate(cutoffTaskEntry, "NichromeCutoff", cutoff_task_stack_size,
                     this, configMAX_PRIORITIES - 1,
                     &cutoff_task_handle_) == pdPASS;
}

bool Nichrome::armCutoff() {
  // 点火開始を監視タスクへ通知し、最大点火時間の計測を開始する。
  if (cutoff_task_handle_ == nullptr) return false;
  cutoff_armed_ = true;
  if (xTaskNotify(cutoff_task_handle_, 1, eSetValueWithOverwrite) == pdPASS) {
    return true;
  }
  cutoff_armed_ = false;
  return false;
}

void Nichrome::cutoffTaskEntry(void* instance) {
  Nichrome* nichrome = static_cast<Nichrome*>(instance);
  for (;;) {
    // 点火開始通知を待ち、通知後に許容点火時間だけ待機する。
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(NichromeSequence::ignition_ms));
    if (nichrome->cutoff_armed_) nichrome->cutoffFromWatchdog();
  }
}

void Nichrome::cutoffFromWatchdog() {
  // 点火時間上限を超えたため、状態機械の処理を待たずに出力を強制遮断する。
  cutoff_armed_ = false;
  cutoff_triggered_ = true;
  forceSafeOutput();
}

Nichrome::SampleTimer::SampleTimer(INA226& ina_ref, uint8_t unit_id_ref,
                              unsigned interval_ms)
  : process::Timer("NichromeTimer", interval_ms),
    ina_Nichrome_(ina_ref),
    unit_id_(unit_id_ref) {
}

void Nichrome::SampleTimer::callback() {
  // INA226の基本単位（V/A/W）をテレメトリ用のmV/mA/mWへ変換する。
  const int voltage_mV = ina_Nichrome_.getBusVoltage() * 1000;
  const int current_mA = ina_Nichrome_.getCurrent() * 1000;
  const int power_mW = ina_Nichrome_.getPower() * 1000;

  wcpp::Packet packet = newPacket(64);
  packet.telemetry(Powertelemetry_id, Nichrome::component_id, unit_id_, 0xFF,
                   kernel::nextPacketSequence(unit_id_, 0xFF,
                                              Nichrome::component_id,
                                              wcpp::packet_type_mask
                                              | Powertelemetry_id));
  packet.append("Vi").setInt(voltage_mV);
  packet.append("Ii").setInt(current_mA);
  packet.append("Pi").setInt(power_mW);
  packet.append("Ts").setInt(millis());
  sendPacket(packet);
}

}  // namespace component

#undef Nichrome_ISR_ATTR
