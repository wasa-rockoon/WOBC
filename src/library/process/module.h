#pragma once

#include "library/kernel/kernel.h"
#include "process.h"
#include "component.h"

namespace process {

class Module: public Component {
public:
  Module(const char *name, uint8_t id) : Component(name, id) {};
  void main();

};

}
