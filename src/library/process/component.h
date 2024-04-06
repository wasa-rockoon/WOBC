#pragma once

#include "process.h"
#include "library/kernel/listener.h"

namespace process {

class Component: public Process {
public:
  Component(const char* name, uint8_t id, unsigned command_queue_size = 5);

protected:
  virtual void setup() {}
  virtual void loop() {}
  virtual void onCommand(const wcpp::Packet& command) {};

private:
  uint8_t id_; 
  uint8_t priority_;
  unsigned stack_size_;

  xTaskHandle handle_;
  kernel::Listener command_listener_;

  void onStart() override;
  static void entryPoint(void* instance);
};

}

