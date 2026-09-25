#include "src/components/Nichrome/NichromeStartGate.h"
#include "src/components/Nichrome/NichromeSequence.h"
#include <cassert>
#include <cstdio>
using component::NichromeStartGate;
using component::NichromeSequence;
constexpr uint32_t hour = 3600000;

static void arm(NichromeStartGate& gate, uint32_t now) {
  gate.observeFlightPin(true, 1);
  assert(gate.updateFlightPin(false, now));
  gate.observeFlightPin(true, 0);
  assert(gate.updateFlightPin(true, now));
}
int main() {
  NichromeStartGate gate;
  gate.observeFlightPin(true, 0);
  gate.updateFlightPin(true, 0);
  assert(!gate.ready(hour, hour));
  arm(gate, 100);
  assert(!gate.ready(30100, hour)); // No unconditional 30-second start.
  assert(!gate.ready(hour + 99, hour));
  assert(gate.ready(hour + 100, hour)); // No pressure ever received; no GPS needed.
  assert(!gate.ready(60099, 0));
  assert(gate.ready(60100, 0)); // Pressure timeout is independently required.

  arm(gate, 100);
  for (uint32_t i=1; i<=30; ++i) gate.observePressure(true, 20000, 100+i*1000, 100+i*1000);
  assert(!gate.ready(30100, hour)); // Strictly greater than 20 km.
  for (uint32_t i=1; i<=29; ++i) gate.observePressure(true, 20001, 30100+i*1000, 30100+i*1000);
  assert(!gate.ready(59100, hour));
  gate.observePressure(true, 20001, 59100, 59100); // Duplicate cannot advance count.
  assert(!gate.ready(59100, hour));
  gate.observePressure(true, 20001, 60100, 60100);
  assert(gate.ready(60100, hour)); // Altitude path needs no hour or GPS.
  gate.observeFlightPin(true, 1);
  gate.observeFlightPin(true, 0);
  assert(gate.updateFlightPin(true, 61000));
  assert(!gate.ready(61000, hour));

  arm(gate, 100);
  gate.observePressure(true, 100, hour+100, hour+100);
  assert(!gate.ready(hour+100, hour)); // Healthy low altitude suppresses fallback.
  gate.observePressure(false, 0, 0, hour+60100-1);
  assert(!gate.ready(hour+60100-1, hour));
  assert(gate.ready(hour+60100, hour)); // Invalid readings don't refresh clock.
  gate.observePressure(true, 100, hour+60101, hour+60101);
  assert(!gate.ready(hour+60101, hour)); // Recovery suppresses fallback.
  gate.updateFlightPin(false, hour+60102);
  assert(!gate.ready(2*hour, hour));

  arm(gate, 100000);
  for (uint32_t i=1; i<=30; ++i) gate.observePressure(true, 21000, i*1000, 100000);
  assert(!gate.ready(100000, hour)); // Pre-removal samples cannot trigger.
  for (uint32_t i=1; i<=29; ++i) gate.observePressure(true, 21000, 100000+i*1000, 100000+i*1000);
  gate.observePressure(false, 0, 0, 130000);
  gate.observePressure(true, 21000, 131000, 131000);
  assert(!gate.ready(131000, hour));
  gate.observeFlightPin(false, 0);
  gate.updateFlightPin(true, 132000);
  assert(!gate.ready(2*hour, hour));

  const uint32_t wrap = UINT32_MAX - 10000;
  arm(gate, wrap);
  assert(!gate.ready(wrap+hour-1, hour));
  assert(gate.ready(wrap+hour, hour));

  // Abort each active phase; no output or restart is allowed afterwards.
  for (unsigned phase = 0; phase < 4; ++phase) {
    NichromeSequence sequence;
    assert(sequence.start(0));
    uint32_t now = 0;
    const uint32_t durations[] = {1000, 30000, 5000};
    for (unsigned i = 0; i < phase; ++i) {
      now += durations[i];
      sequence.update(now);
    }
    sequence.abort(now);
    const auto snapshot = sequence.update(now + 200000);
    assert(snapshot.phase == NichromeSequence::Phase::Disarmed);
    assert(!snapshot.high && !snapshot.low);
    assert(!sequence.start(now + 200001));
  }
  static_assert(NichromeSequence::countdown_ms == 30000, "Preserve countdown");
  static_assert(NichromeSequence::ignition_ms == 120000, "Preserve heating time");
  std::puts("Nichrome FlightPin tests passed");
}
