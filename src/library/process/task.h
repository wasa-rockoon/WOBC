#pragma once

#include "process.h"

namespace process {

class Task: public Process {
public:
  Task(const char* name, unsigned stack_size = 1600, uint8_t priority = 0);
  inline void terminate() { terminated_ = true; }

protected:
  virtual void setup() {} // Override this
  virtual void loop() {} // Override this

private:
  bool     terminated_;
  unsigned stack_size_;
  uint8_t  priority_;
  xTaskHandle task_handle_;

  bool onStart() override;

  static void entryPoint(void* instance);
};

}
