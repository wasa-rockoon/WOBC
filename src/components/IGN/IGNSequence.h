#pragma once

#include <stdint.h>

namespace component {

// 時間管理と出力判定だけを担う、ハードウェア非依存の点火状態機械。
// GPIO操作から分離することで、安全条件をPC向けテストでも検証できる。
class IGNSequence {
public:
  enum class Phase : uint8_t {
    // 起動警告、カウントダウン、最終警告、点火、完了、待機、異常の各段階。
    Startup   = 0,
    Countdown = 1,
    Final     = 2,
    Ignition  = 3,
    Done      = 4,
    Disarmed  = 5,
    Fault     = 6,
  };

  struct Snapshot {
    Phase phase;
    uint32_t phase_elapsed_ms;
    uint32_t sequence_elapsed_ms;
    uint32_t remaining_ms;
    bool high;
    bool low;
    bool status_led;
    bool phase_changed;
  };

  // 各段階の時間と、ブザー／LEDの点滅周期をミリ秒で定義する。
  static constexpr uint32_t startup_buzz_ms = 1000;
  static constexpr uint32_t countdown_ms = 3000;
  static constexpr uint32_t countdown_beep_period_ms = 1000;
  static constexpr uint32_t countdown_beep_on_ms = 200;
  static constexpr uint32_t final_buzz_ms = 5000;
  static constexpr uint32_t ignition_ms = 60000;
  static constexpr uint32_t final_blink_period_ms = 200;
  static constexpr uint32_t done_blink_period_ms = 2000;
  static constexpr uint32_t done_blink_on_ms = 100;
  static constexpr uint32_t fault_blink_period_ms = 500;

  IGNSequence()
    : phase_(Phase::Disarmed),
      phase_start_ms_(0),
      sequence_start_ms_(0),
      has_started_(false),
      phase_changed_(true) {}

  bool start(uint32_t now_ms) {
    // 一度の起動中に開始できるのは一回だけ。中止後や重複要求による
    // 二度目の点火を防ぐ。
    if (phase_ != Phase::Disarmed || has_started_) return false;
    has_started_ = true;
    sequence_start_ms_ = now_ms;
    enter(Phase::Startup, now_ms);
    return true;
  }

  void abort(uint32_t now_ms) {
    // 出力を要求しない待機状態に戻す（再開始はhas_started_で禁止される）。
    enter(Phase::Disarmed, now_ms);
  }

  void fault(uint32_t now_ms) {
    // 異常状態へ遷移し、点滅LED以外の出力要求を停止する。
    enter(Phase::Fault, now_ms);
  }

  void cutoff(uint32_t now_ms) {
    // 監視タスクによる時間超過遮断は、点火中の場合だけ完了として受け取る。
    if (phase_ == Phase::Ignition) enter(Phase::Done, now_ms);
  }

  Phase phase() const { return phase_; }
  bool hasStarted() const { return has_started_; }

  Snapshot update(uint32_t now_ms) {
    const uint32_t elapsed_ms = phaseElapsed(now_ms);

    // 1回の呼び出しで進める段階は最大1つ。タスクが遅延しても警告段階を
    // 飛ばして直ちに点火することがないよう、残りの警告時間を必ず実行する。
    switch (phase_) {
      case Phase::Startup:
        if (elapsed_ms >= startup_buzz_ms) enter(Phase::Countdown, now_ms);
        break;
      case Phase::Countdown:
        if (elapsed_ms >= countdown_ms) enter(Phase::Final, now_ms);
        break;
      case Phase::Final:
        if (elapsed_ms >= final_buzz_ms) enter(Phase::Ignition, now_ms);
        break;
      case Phase::Ignition:
        // 通電要求を返す前に期限を確認する。最初のupdate()が遅れても、
        // 期限後に遅れて点火パルスを出すことはない。
        if (elapsed_ms >= ignition_ms) enter(Phase::Done, now_ms);
        break;
      case Phase::Done:
      case Phase::Disarmed:
      case Phase::Fault:
        break;
      default:
        enter(Phase::Fault, now_ms);
        break;
    }

    Snapshot result = makeSnapshot(now_ms);
    result.phase_changed = phase_changed_;
    phase_changed_ = false;
    return result;
  }

private:
  Phase phase_;
  uint32_t phase_start_ms_;
  uint32_t sequence_start_ms_;
  bool has_started_;
  bool phase_changed_;

  void enter(Phase phase, uint32_t now_ms) {
    // 遷移時刻を記録し、次のSnapshotに段階変更フラグを付与する。
    phase_ = phase;
    phase_start_ms_ = now_ms;
    phase_changed_ = true;
  }

  uint32_t phaseElapsed(uint32_t now_ms) const {
    // unsignedの減算により、millis()のオーバーフロー後も経過時間を計算できる。
    return now_ms - phase_start_ms_;
  }

  static uint32_t remainingInPhase(uint32_t total_ms, uint32_t elapsed_ms) {
    // 段階の残り時間が負にならないよう0で打ち止めにする。
    return elapsed_ms >= total_ms ? 0 : total_ms - elapsed_ms;
  }

  Snapshot makeSnapshot(uint32_t now_ms) const {
    // 現在段階から、GPIOの要求状態・LED表示・残り時間を組み立てる。
    Snapshot result = {};
    result.phase = phase_;
    result.phase_elapsed_ms = phaseElapsed(now_ms);
    result.sequence_elapsed_ms = has_started_ ? now_ms - sequence_start_ms_ : 0;

    switch (phase_) {
      case Phase::Startup:
        // 起動直後はブザーとLEDを連続で有効にする。
        result.high = true;
        result.status_led = true;
        result.remaining_ms = remainingInPhase(startup_buzz_ms, result.phase_elapsed_ms)
                            + countdown_ms + final_buzz_ms;
        break;
      case Phase::Countdown:
        // カウントダウン中は、短いビープとLED点滅で残り時間を警告する。
        result.high = (result.phase_elapsed_ms % countdown_beep_period_ms)
                    < countdown_beep_on_ms;
        result.status_led = result.high;
        result.remaining_ms = remainingInPhase(countdown_ms, result.phase_elapsed_ms)
                            + final_buzz_ms;
        break;
      case Phase::Final:
        // 最終警告ではブザーを連続にし、LEDを高速点滅させる。
        result.high = true;
        result.status_led = (result.phase_elapsed_ms % final_blink_period_ms)
                          < (final_blink_period_ms / 2);
        result.remaining_ms = remainingInPhase(final_buzz_ms, result.phase_elapsed_ms);
        break;
      case Phase::Ignition:
        // HIGHとLOWの両方を要求して点火回路を通電する。
        result.high = true;
        result.low = true;
        result.status_led = true;
        break;
      case Phase::Done:
        // 点火後は短い周期でLEDを点滅させ、完了状態を示す。
        result.status_led = (result.phase_elapsed_ms % done_blink_period_ms)
                          < done_blink_on_ms;
        break;
      case Phase::Fault:
        // 異常時はLEDの高速点滅のみを要求し、点火出力は出さない。
        result.status_led = (result.phase_elapsed_ms % fault_blink_period_ms)
                          < (fault_blink_period_ms / 2);
        break;
      case Phase::Disarmed:
      default:
        break;
    }

    return result;
  }
};

}  // namespace component
