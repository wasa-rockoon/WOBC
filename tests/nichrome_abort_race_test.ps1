# Compile the production request/ISR/arming code with host GPIO/RTOS shims.
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$source = Get-Content -Raw -Encoding utf8 -LiteralPath (Join-Path $repo 'src/components/Nichrome/Nichrome.cpp')
$request = [regex]::Match($source, '(?s)bool Nichrome::startSequence\(\).*?(?=void Nichrome::abortSequence\()').Value
$isr = [regex]::Match($source, '(?s)void Nichrome_ISR_ATTR Nichrome::abortSequenceFromISR\(\).*?(?=void Nichrome::setup\()').Value
$arming = [regex]::Match($source, '(?s)  const bool sequence_active =.*?portEXIT_CRITICAL\(&output_mux_\);').Value
if (!$request -or !$isr -or !$arming) { throw 'Production test sections not found' }
$prefix = @'
#include "src/components/Nichrome/NichromeSequence.h"
#include <cassert>
#include <cstdio>
#define Nichrome_ISR_ATTR
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
#define portENTER_CRITICAL_ISR(x) ((void)0)
#define portEXIT_CRITICAL_ISR(x) ((void)0)
constexpr int LOW = 0;
int gpio[2] = {1, 1};
void digitalWrite(int pin, int value) { gpio[pin] = value; }
using component::NichromeSequence;
class Nichrome {
public:
  using Phase = NichromeSequence::Phase;
  NichromeSequence sequence_;
  bool begin_ok_ = true, start_requested_ = false, abort_requested_ = false;
  bool cutoff_armed_ = false, flight_pin_abort_armed_ = false;
  bool low_out_ = true, high_out_ = true, status_changed_ = false;
  int low_pin_ = 0, high_pin_ = 1;
  bool startSequence();
  void abortSequenceFromISR();
  void refreshAbort(const NichromeSequence::Snapshot& snapshot);
};
'@
$test = @'
int main() {
  Nichrome n;
  const auto stale = n.sequence_.update(0); // Loop has already captured Disarmed.
  assert(n.startSequence());              // Main reserves a start meanwhile.
  n.refreshAbort(stale);                  // Loop resumes with the old snapshot.
  assert(n.flight_pin_abort_armed_);      // Must not lose protection.
  n.abortSequenceFromISR();               // Pin insertion before next loop.
  assert(n.abort_requested_ && !n.start_requested_);
  assert(!n.low_out_ && !n.high_out_ && gpio[0] == 0 && gpio[1] == 0);
  n.sequence_.start(1);
  n.refreshAbort(n.sequence_.update(1));
  assert(!n.flight_pin_abort_armed_);      // Pending abort cannot be rearmed.
  Nichrome idle;
  idle.refreshAbort(idle.sequence_.update(0));
  idle.abortSequenceFromISR();
  assert(!idle.abort_requested_);         // Idle insertion remains ignored.
  std::puts("Nichrome pending-start abort race tests passed");
}
'@
$generated = Join-Path $repo '.pio/nichrome_abort_race_test.cpp'
$binary = Join-Path $repo '.pio/nichrome_abort_race_test.exe'
[IO.Directory]::CreateDirectory((Join-Path $repo '.pio')) | Out-Null
$code = $prefix + "`n" + $request + "`n" + $isr + "`nvoid Nichrome::refreshAbort(const NichromeSequence::Snapshot& snapshot) {`n" + $arming + "`n}`n" + $test
try {
  [IO.File]::WriteAllText($generated, $code, [Text.UTF8Encoding]::new($false))
  & g++ -std=c++14 -Wall -Wextra -Werror -I $repo $generated -o $binary
  if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
  & $binary
  if ($LASTEXITCODE -ne 0) { throw 'Abort race regression failed' }
} finally {
  Remove-Item -LiteralPath $generated, $binary -ErrorAction SilentlyContinue
}
