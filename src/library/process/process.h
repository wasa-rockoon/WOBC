#pragma once

#include <FreeRTOS.h>
#include <task.h>
#include "library/kernel/kernel.h"
#include "library/kernel/listener.h"

namespace process {

class Process {
public:
  using Listener = kernel::Listener;

  Process(const char* name) : name_(name){};

  inline void delay(unsigned ms) { vTaskDelay(ms); }

  void listen(kernel::Listener &listener, unsigned queue_size = 1, bool force_push = true);

  inline void start(Process& sub_process) { sub_process.startProcess(kernel_); };

  wcpp::Packet packet(uint8_t size);
  void send(wcpp::Packet &packet);



  // void readROM();
  // void writeROM();

protected:
  const char *name_;
  uint8_t core_;

  kernel::Kernel *kernel_;

  void startProcess(kernel::Kernel* kernel);
  virtual void onStart() = 0;
};

}

