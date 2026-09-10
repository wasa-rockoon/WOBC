#include "src/components/IGN/IGNAltitudeGate.h"
#include "src/components/IGN/IGNSequence.h"
#include <cassert>
#include <cstdio>

int main() {
  component::IGNAltitudeGate gate(15000);
  assert(!gate.ready());
  for (unsigned i = 0; i < 29; ++i) assert(!gate.observe(true, 15001));
  assert(gate.observe(true, 15001));
  for (unsigned i = 0; i < 300; ++i) assert(gate.observe(true, 16000));

  // Equality resets the streak; all 30 observations must be above the threshold.
  assert(!gate.observe(true, 15000));
  for (unsigned i = 0; i < 29; ++i) assert(!gate.observe(true, 15001));
  assert(!gate.observe(true, 14999));
  for (unsigned i = 0; i < 29; ++i) assert(!gate.observe(true, 15001));
  assert(!gate.observe(false, 20000));
  for (unsigned i = 0; i < 29; ++i) assert(!gate.observe(true, 15001));
  assert(gate.observe(true, 15001));

  // Reinsertion resets qualification, including a completed streak.
  gate.reset();
  assert(!gate.ready());
  for (unsigned i = 0; i < 29; ++i) assert(!gate.observe(true, 15001));
  assert(gate.observe(true, 15001));

  component::IGNAltitudeGate custom(20000);
  for (unsigned i = 0; i < 30; ++i) assert(!custom.observe(true, 20000));
  for (unsigned i = 0; i < 29; ++i) assert(!custom.observe(true, 20001));
  assert(custom.observe(true, 20001));

  // Qualification enters the existing warning sequence; abort prevents restart.
  component::IGNSequence sequence;
  assert(sequence.start(0));
  assert(sequence.phase() == component::IGNSequence::Phase::Startup);
  sequence.update(1000);
  assert(sequence.phase() == component::IGNSequence::Phase::Countdown);
  sequence.update(61000);
  assert(sequence.phase() == component::IGNSequence::Phase::Final);
  sequence.update(66000);
  assert(sequence.phase() == component::IGNSequence::Phase::Ignition);
  sequence.abort(66001);
  const auto stopped = sequence.update(66001);
  assert(!stopped.high && !stopped.low);
  assert(!sequence.start(67000));
  std::puts("IGN altitude and sequence tests passed");
}
