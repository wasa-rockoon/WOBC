#pragma once

#include "library/kernel/kernel.h"
#include "library/kernel/listener.h"

namespace process {

class Component;

class Process {
public:
  using Listener = kernel::Listener;

  Process(const char* name) : name_(name){};

  inline void delay(unsigned ms) { vTaskDelay(ms); }

  void listen(kernel::Listener &listener, unsigned queue_size = 1, bool force_push = true);

  inline void start(Process& sub_process) { sub_process.startProcess(component_); };

  Component& component() const { return *component_; };

  wcpp::Packet newPacket(uint8_t size);
  wcpp::Packet decodePacket(const uint8_t* buf);
  void sendPacket(const wcpp::Packet &packet);
  void sendPacket(const wcpp::Packet &packet, const Listener& exclude);


protected:
  const char *name_;
  Component* component_;
  uint8_t core_;

  void startProcess(Component* component);
  virtual bool onStart() = 0;
};

}

