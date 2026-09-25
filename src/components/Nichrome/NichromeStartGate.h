#pragma once

#include <stdint.h>

namespace component {

// Mainタスク専用。抜去後の気圧高度、または気圧更新停止＋時間で開始する。
class NichromeStartGate {
public:
  static constexpr uint32_t pressure_timeout_ms = 60000;
  static constexpr unsigned required_samples = 30;
  explicit NichromeStartGate(int32_t altitude_m = 20000) : altitude_m_(altitude_m) {}
  void observeFlightPin(bool valid, int64_t state) {
    if (!valid || (state != 0 && state != 1)) {
      removed_ = false;
      timing_ = false;
    } else if (state == 1) {
      inserted_seen_ = true;
      removed_ = false;
      timing_ = false;
    } else {
      removed_ = inserted_seen_;
    }
  }

  // trueなら挿入中または新たな抜去。呼び出し側も保留気圧パケットを破棄する。
  bool updateFlightPin(bool pin_is_low, uint32_t now_ms) {
    if (!removed_ || !pin_is_low) {
      timing_ = false;
    } else if (timing_) {
      return false;
    } else {
      timing_ = true;
    }
    removed_ms_ = now_ms;
    last_pressure_ms_ = now_ms;
    pressure_seen_ = false;
    count_ = 0;
    return true;
  }

  void observePressure(bool valid, int64_t altitude_m,
                       uint32_t sample_ms, uint32_t now_ms) {
    if (!timing_) return;
    if (!valid || now_ms - sample_ms >= pressure_timeout_ms
        || now_ms - sample_ms > now_ms - removed_ms_) {
      count_ = 0;
      return;
    }
    const uint32_t delta = sample_ms - last_pressure_ms_;
    if (pressure_seen_ && (delta == 0 || delta >= 0x80000000UL)) return;
    pressure_seen_ = true;
    last_pressure_ms_ = sample_ms;
    if (altitude_m <= altitude_m_) count_ = 0;
    else if (count_ < required_samples) ++count_;
  }

  bool ready(uint32_t now_ms, uint32_t delay_ms) const {
    return timing_ && (count_ == required_samples
        || (now_ms - removed_ms_ >= delay_ms
            && now_ms - last_pressure_ms_ >= pressure_timeout_ms));
  }

private:
  bool inserted_seen_ = false;
  bool removed_ = false;
  bool timing_ = false;
  uint32_t removed_ms_ = 0;
  const int32_t altitude_m_;
  uint32_t last_pressure_ms_ = 0;
  bool pressure_seen_ = false;
  unsigned count_ = 0;
};

}  // namespace component
