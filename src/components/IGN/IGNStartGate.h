#pragma once

#include "IGNAltitudeGate.h"

namespace component {

// Mainタスク専用。GPIOや受信処理から独立したシーケンス開始条件。
class IGNStartGate {
public:
  static constexpr uint32_t pressure_timeout_ms = 60000;
  static constexpr uint32_t gps_max_age_ms = 5000;
  static constexpr int32_t gps_min_altitude_m = 10000;

  explicit IGNStartGate(int32_t altitude_m) : altitude_gate_(altitude_m) {}

  // 挿入中・抜去直後はtrue。呼び出し側も保留パケットを破棄する。
  bool setFlightPinRemoved(bool removed, uint32_t now_ms) {
    if (removed && armed_) return false;
    armed_ = removed;
    removed_ms_ = now_ms;
    last_pressure_ms_ = now_ms;
    pressure_seen_ = false;
    gps_seen_ = false;
    gps_valid_ = false;
    altitude_gate_.reset();
    return true;
  }

  // sample_msはIGN自身のmillis()。有効な新規測定だけで時計を更新する。
  void observePressure(bool valid, int64_t altitude_m,
                       uint32_t sample_ms, uint32_t now_ms) {
    if (!armed_) return;
    if (!valid || now_ms - sample_ms >= pressure_timeout_ms
        || now_ms - sample_ms > now_ms - removed_ms_) {
      altitude_gate_.reset();
      return;
    }
    if (pressure_seen_ && !newer(sample_ms, last_pressure_ms_)) return;
    pressure_seen_ = true;
    last_pressure_ms_ = sample_ms;
    altitude_gate_.observe(true, altitude_m);
  }

  // 既存GPSのUTが進んだ受信だけを更新として扱う。高度自体の測定鮮度は
  // 既存パケットからは不明。キュー滞留と受信後の経過を5秒未満に制限する。
  void observeGPS(bool valid, int64_t altitude_m, uint32_t queued_ms,
                  uint64_t utc_key, uint32_t now_ms) {
    if (!armed_) return;
    if (!valid) {
      gps_valid_ = false;
      return;
    }
    if (gps_seen_ && utc_key <= last_gps_utc_) return;
    gps_seen_ = true;
    last_gps_utc_ = utc_key;
    gps_received_ms_ = now_ms;
    gps_queued_ms_ = queued_ms;
    gps_valid_ = queued_ms < gps_max_age_ms && altitude_m >= gps_min_altitude_m;
  }

  bool pressureReady() const { return armed_ && altitude_gate_.ready(); }
  bool pressureStale(uint32_t now_ms) const {
    return armed_ && now_ms - last_pressure_ms_ >= pressure_timeout_ms;
  }
  bool fallbackReady(uint32_t now_ms, uint32_t delay_ms) const {
    return armed_ && now_ms - removed_ms_ >= delay_ms
        && pressureStale(now_ms) && gps_valid_
        && gps_queued_ms_ < gps_max_age_ms
        && now_ms - gps_received_ms_ < gps_max_age_ms - gps_queued_ms_;
  }
  bool ready(uint32_t now_ms, uint32_t delay_ms) const {
    return pressureReady() || fallbackReady(now_ms, delay_ms);
  }

private:
  static bool newer(uint32_t value, uint32_t previous) {
    const uint32_t delta = value - previous;
    return delta != 0 && delta < 0x80000000UL;
  }

  IGNAltitudeGate altitude_gate_;
  bool armed_ = false;
  bool pressure_seen_ = false;
  bool gps_seen_ = false;
  bool gps_valid_ = false;
  uint32_t removed_ms_ = 0;
  uint32_t last_pressure_ms_ = 0;
  uint64_t last_gps_utc_ = 0;
  uint32_t gps_received_ms_ = 0;
  uint32_t gps_queued_ms_ = 0;
};

}  // namespace component
