#pragma once

#include "process.h"
#include "library/kernel/listener.h"

namespace process {

#ifndef WOBC_COMPONENT_STORE_COMMANDS_MAX
#define WOBC_COMPONENT_STORE_COMMANDS_MAX 8
#endif

class Component: public Process {
public:
  Component(const char* name, uint8_t id, unsigned command_queue_size = 5, 
            unsigned stack_size = 4096);

  bool begin();
  bool storeOnCommand(uint8_t packet_id);

protected:
  virtual void setup() {}
  virtual void loop() {}
  virtual void onCommand(const wcpp::Packet& command) {};

  uint8_t id_; 
  uint8_t priority_;
  unsigned stack_size_;

private:
  xTaskHandle task_handle_;
  kernel::Listener command_listener_;
  unsigned command_queue_size_;
  uint8_t store_command_ids[WOBC_COMPONENT_STORE_COMMANDS_MAX];

  bool onStart() override;
  static void entryPoint(void* instance);
};

}

