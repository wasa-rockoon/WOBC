#pragma once

#include "library/kernel/kernel.h"
#include "process.h"
#include "component.h"

namespace process {

class Module: public Process {
public:
  Module(const char *name) : Process(name) {};
  void main();

protected:
  virtual void setup() {};

private:
  kernel::Kernel kernel_;

  void onStart() override {};
};

}
