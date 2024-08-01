#include "component.h"

namespace process {


Component::Component(const char* name, uint8_t id, unsigned command_queue_size, unsigned stack_size)
: Process(name), id_(id), stack_size_(stack_size) {
  component_ = this;
  command_listener_.command().component(id);
}


bool Component::begin() {
  command_listener_.begin();
  return onStart();
}

bool Component::onStart() {
  return xTaskCreatePinnedToCore(
    entryPoint, name_, stack_size_, this,
    priority_, &handle_, 1) == pdPASS;
}

void Component::entryPoint(void* instance) {
  Component *component = static_cast<Component *>(instance);
  component->setup();
  taskYIELD();
  for (;;) {
    while (component->command_listener_.available()) {
      const wcpp::Packet command = component->command_listener_.pop();
      component->onCommand(command);
      taskYIELD();
    }
    component->loop();
    taskYIELD();
  }
}
}