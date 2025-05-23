#include "timer.h"

namespace process {

Timer::Timer(const char* name, unsigned period_ms, bool auto_reload) 
  : Process(name) {
  timer_handle_ = xTimerCreate(
    name_, period_ms, auto_reload ? pdTRUE : pdFALSE, this, entryPoint);
}

Timer::~Timer() {
  xTimerDelete(timer_handle_, 0);
}

bool Timer::stop() {
  return xTimerStop(timer_handle_, 0) == pdPASS;
}

bool Timer::start() {
  return xTimerStart(timer_handle_, 0) == pdPASS;
}

bool Timer::reset() {
  return xTimerReset(timer_handle_, 0) == pdPASS;
}

void Timer::setReload(bool auto_reload) {
  vTimerSetReloadMode(timer_handle_, auto_reload ? pdTRUE : pdFALSE);
}

void Timer::changePeriod(unsigned ms) {
  xTimerChangePeriod(timer_handle_, ms, 0);
}

bool Timer::onStart() {
  return start();
}

void Timer::entryPoint(TimerHandle_t timer_handle) {
  ((Timer*)pvTimerGetTimerID(timer_handle))->callback();
}


}