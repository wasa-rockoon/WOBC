#pragma once

#include <stdint.h>

namespace component {

// ピン抜去後の有効な高度サンプルを連続して確認する。
class IGNAltitudeGate {
public:
  explicit IGNAltitudeGate(int32_t altitude_m) : altitude_m_(altitude_m) {}

  void reset() { count_ = 0; }
  bool observe(bool valid, int64_t altitude_m) {
    if (!valid || altitude_m <= altitude_m_) {
      reset();
    } else if (count_ < required_samples) {
      ++count_;
    }
    return ready();
  }
  bool ready() const { return count_ == required_samples; }

private:
  static constexpr unsigned required_samples = 30;
  const int32_t altitude_m_;
  unsigned count_ = 0;
};

}  // namespace component
