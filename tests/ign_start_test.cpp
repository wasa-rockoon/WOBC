#include "src/components/IGN/IGNStartGate.h"
#include "src/components/IGN/IGNGPSTime.h"
#include <cassert>
#include <cstdio>

using component::IGNStartGate;
constexpr uint32_t hour = 60UL * 60UL * 1000UL;

static void normal_path() {
  IGNStartGate gate(15000);
  gate.observePressure(true, 20000, 0, 0);
  gate.observeGPS(true, 20000, 0, 0, 0);
  assert(!gate.ready(hour, hour));
  assert(gate.setFlightPinRemoved(true, 100));
  for (uint32_t i = 1; i <= 29; ++i) {
    gate.observePressure(true, 15000, 100 + i * 1000, 100 + i * 1000);
    assert(!gate.ready(100 + i * 1000, hour));
  }
  // Polls and duplicate packets cannot stand in for new measurements.
  for (unsigned i = 0; i < 100; ++i) {
    gate.observePressure(true, 15000, 29100, 30000);
    assert(!gate.ready(30000, hour));
  }
  gate.observePressure(true, 15000, 30100, 30100);
  assert(gate.ready(30100, hour)); // Normal path needs neither GPS nor one hour.
  assert(gate.setFlightPinRemoved(false, 30101));
  assert(!gate.ready(hour, hour));

  gate.setFlightPinRemoved(true, 40000);
  for (uint32_t i = 1; i <= 29; ++i) gate.observePressure(true, 16000, 40000 + i, 40000 + i);
  gate.observePressure(false, 16000, 40030, 40030);
  gate.observePressure(true, 16000, 40031, 40031);
  assert(!gate.ready(40031, hour));
  gate.observePressure(true, 14999, 40032, 40032);
  for (uint32_t i = 1; i <= 29; ++i) gate.observePressure(true, 16000, 40032 + i, 40032 + i);
  assert(!gate.ready(40061, hour));
  gate.observePressure(true, 16000, 40062, 40062);
  assert(gate.ready(40062, hour));
}

static void fallback_boundaries() {
  IGNStartGate gate(15000);
  gate.setFlightPinRemoved(true, 100);
  assert(!gate.pressureStale(60099));
  assert(gate.pressureStale(60100)); // No sample since removal also times out.
  gate.observeGPS(true, 10000, 0, 7, 100 + hour - 1);
  assert(!gate.ready(100 + hour - 1, hour));
  assert(gate.ready(100 + hour, hour));
  assert(!gate.ready(100 + hour + 4999, hour)); // Exactly 5 seconds old.
  // Replaying the same source packet cannot refresh its receive time.
  gate.observeGPS(true, 20000, 0, 7, 100 + hour + 5000);
  assert(!gate.ready(100 + hour + 5000, hour));
  gate.observeGPS(true, 9999, 0, 8, 100 + hour + 5001);
  assert(!gate.ready(100 + hour + 5001, hour));
  gate.observeGPS(true, 10000, 0, 9, 100 + hour + 5002);
  assert(gate.ready(100 + hour + 5002, hour));
  gate.observeGPS(false, 10000, 0, 10, 100 + hour + 5003);
  assert(!gate.ready(100 + hour + 5003, hour));
  gate.observeGPS(true, 10000, 5000, 11, 100 + hour + 5004);
  assert(!gate.ready(100 + hour + 5004, hour));
  gate.observeGPS(true, 10000, UINT32_MAX, 12, 100 + hour + 5005);
  assert(!gate.ready(100 + hour + 5005, hour));
  gate.observeGPS(true, 10000, 4999, 13, 100 + hour + 5006);
  assert(gate.ready(100 + hour + 5006, hour));
  assert(!gate.ready(100 + hour + 5007, hour));
}

static void all_fallback_terms_required() {
  // Cover every combination of stale pressure, expired timer and qualifying GPS.
  for (unsigned mask = 0; mask < 8; ++mask) {
    const bool stale = mask & 1;
    const bool expired = mask & 2;
    const bool gps_high = mask & 4;
    IGNStartGate gate(15000);
    gate.setFlightPinRemoved(true, 0);
    const uint32_t now = expired ? hour : hour - 1;
    if (!stale) gate.observePressure(true, 9000, now, now);
    gate.observeGPS(true, gps_high ? 10000 : 9999, 0, 100, now);
    assert(gate.ready(now, hour) == (stale && expired && gps_high));
  }
  IGNStartGate gate(15000);
  gate.setFlightPinRemoved(true, 0);
  assert(!gate.ready(hour, hour)); // GPS never received.
}

static void pressure_health_and_rearming() {
  IGNStartGate gate(15000);
  gate.setFlightPinRemoved(true, 0);
  gate.observePressure(true, 9000, hour, hour);
  gate.observeGPS(true, 10000, 0, 1, hour + 59999);
  assert(!gate.ready(hour + 59999, hour));
  // A failed reading neither updates the clock nor counts toward 30 samples.
  gate.observePressure(false, 20000, hour + 60000, hour + 60000);
  assert(gate.ready(hour + 60000, hour));
  // Recovery, even to an unchanged low altitude, disables the fallback.
  gate.observePressure(true, 9000, hour + 60001, hour + 60001);
  assert(!gate.ready(hour + 60001, hour));
  gate.setFlightPinRemoved(false, hour + 60002);
  gate.setFlightPinRemoved(true, hour + 60003);
  gate.observeGPS(true, 10000, 0, 2, hour + 60004);
  assert(!gate.ready(hour + 60004, hour));
  assert(!gate.pressureStale(hour + 60004));
  // Pre-removal, future, and 60-second-old pressure samples are rejected.
  gate.observePressure(true, 20000, hour, hour + 60004);
  gate.observePressure(true, 20000, hour + 60005, hour + 60004);
  gate.observePressure(true, 20000, hour + 60003, hour + 120003);
  assert(!gate.pressureReady());
  assert(gate.pressureStale(hour + 120003));
}

static void configurable_delay_and_wraparound() {
  const uint32_t start = UINT32_MAX - 10000;
  IGNStartGate gate(15000);
  gate.setFlightPinRemoved(true, start);
  gate.observeGPS(true, 10000, 0, 2026091623595900ULL, start + 60000);
  assert(gate.ready(start + 60000, 60000));
  assert(!gate.ready(start + 60000, hour));
  gate.observeGPS(true, 10000, 0, 2026091700000000ULL, start + hour);
  assert(gate.ready(start + hour, hour)); // IGN clock wraps, GPS date advances.
  gate.observePressure(true, 9000, start + hour, start + hour);
  assert(!gate.ready(start + hour, hour));
  gate.observeGPS(true, 10000, 0, 2026091700010000ULL, start + hour + 60000);
  assert(gate.ready(start + hour + 60000, hour));
}

static void existing_gps_utc() {
  uint64_t key = 0;
  assert(component::parseIGNGPSUtc("2026-09-16 12:34:56.78", key));
  assert(key == 2026091612345678ULL);
  assert(component::parseIGNGPSUtc("2028-02-29 23:59:60.00", key));
  assert(!component::parseIGNGPSUtc("2026-02-29 00:00:00.00", key));
  assert(!component::parseIGNGPSUtc("2000-00-00 00:00:00.00", key));
  assert(!component::parseIGNGPSUtc("2026-09-16 24:00:00.00", key));
  assert(!component::parseIGNGPSUtc("2026-09-16 12:60:00.00", key));
  assert(!component::parseIGNGPSUtc("2026-09-16 12:34:61.00", key));
  assert(!component::parseIGNGPSUtc("2026/09/16 12:34:56.78", key));
  assert(!component::parseIGNGPSUtc("2026-09-16 12:34:56.xx", key));
  assert(!component::parseIGNGPSUtc("", key));
  assert(!component::parseIGNGPSUtc(nullptr, key));
}

int main() {
  normal_path();
  fallback_boundaries();
  all_fallback_terms_required();
  pressure_health_and_rearming();
  configurable_delay_and_wraparound();
  existing_gps_utc();
  std::puts("IGN start condition tests passed");
}
