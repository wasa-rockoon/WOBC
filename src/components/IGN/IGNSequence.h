#pragma once

#include <stdint.h>

namespace component {

// Hardware-independent ignition sequence. Keeping the timing and output
// decisions here makes the safety invariants testable on the native target.
class IGNSequence {
public:
  enum class Phase : uint8_t {
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

  static constexpr uint32_t startup_buzz_ms = 1000;
  static constexpr uint32_t countdown_ms = 30000;
  static constexpr uint32_t countdown_beep_period_ms = 1000;
  static constexpr uint32_t countdown_beep_on_ms = 200;
  static constexpr uint32_t final_buzz_ms = 5000;
  static constexpr uint32_t ignition_ms = 3000;
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
    // A sequence can only be started once per boot. This prevents an abort or
    // duplicate request from causing a second ignition attempt.
    if (phase_ != Phase::Disarmed || has_started_) return false;
    has_started_ = true;
    sequence_start_ms_ = now_ms;
    enter(Phase::Startup, now_ms);
    return true;
  }

  void abort(uint32_t now_ms) {
    enter(Phase::Disarmed, now_ms);
  }

  void fault(uint32_t now_ms) {
    enter(Phase::Fault, now_ms);
  }

  void cutoff(uint32_t now_ms) {
    if (phase_ == Phase::Ignition) enter(Phase::Done, now_ms);
  }

  Phase phase() const { return phase_; }
  bool hasStarted() const { return has_started_; }

  Snapshot update(uint32_t now_ms) {
    const uint32_t elapsed_ms = phaseElapsed(now_ms);

    // At most one phase is advanced per call. If the task was delayed, the
    // remaining warning phases still run for their full duration rather than
    // catching up by immediately energizing the igniter.
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
        // Check the deadline before returning an energized output. A delayed
        // first update can therefore never produce a late ignition pulse.
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
    phase_ = phase;
    phase_start_ms_ = now_ms;
    phase_changed_ = true;
  }

  uint32_t phaseElapsed(uint32_t now_ms) const {
    return now_ms - phase_start_ms_;
  }

  static uint32_t remainingInPhase(uint32_t total_ms, uint32_t elapsed_ms) {
    return elapsed_ms >= total_ms ? 0 : total_ms - elapsed_ms;
  }

  Snapshot makeSnapshot(uint32_t now_ms) const {
    Snapshot result = {};
    result.phase = phase_;
    result.phase_elapsed_ms = phaseElapsed(now_ms);
    result.sequence_elapsed_ms = has_started_ ? now_ms - sequence_start_ms_ : 0;

    switch (phase_) {
      case Phase::Startup:
        result.high = true;
        result.status_led = true;
        result.remaining_ms = remainingInPhase(startup_buzz_ms, result.phase_elapsed_ms)
                            + countdown_ms + final_buzz_ms;
        break;
      case Phase::Countdown:
        result.high = (result.phase_elapsed_ms % countdown_beep_period_ms)
                    < countdown_beep_on_ms;
        result.status_led = result.high;
        result.remaining_ms = remainingInPhase(countdown_ms, result.phase_elapsed_ms)
                            + final_buzz_ms;
        break;
      case Phase::Final:
        result.high = true;
        result.status_led = (result.phase_elapsed_ms % final_blink_period_ms)
                          < (final_blink_period_ms / 2);
        result.remaining_ms = remainingInPhase(final_buzz_ms, result.phase_elapsed_ms);
        break;
      case Phase::Ignition:
        result.high = true;
        result.low = true;
        result.status_led = true;
        break;
      case Phase::Done:
        result.status_led = (result.phase_elapsed_ms % done_blink_period_ms)
                          < done_blink_on_ms;
        break;
      case Phase::Fault:
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
