#pragma once

#include "process.h"

namespace process {

class Timer : public Process {
public:
  Timer(const char* name, unsigned period_ms, bool auto_reload = true);
  ~Timer();

  bool stop();
  bool start();
  bool reset();

  void setReload(bool auto_reload);
  void changePeriod(unsigned ms);


protected:
  virtual void callback() {} // Overwrite this

private:
  xTimerHandle timer_handle_;

  bool onStart() override;

  static void entryPoint(xTimerHandle timer_handle);
};

}
