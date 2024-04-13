#include "component.h"

namespace process {


Component::Component(const char* name, uint8_t id, unsigned command_queue_size)
: Process(name), id_(id) {
    command_listener_.command().component(id);
}

void Component::onStart() {
  xTaskCreate(
    entryPoint, name_, stack_size_ * sizeof(size_t), this,
    priority_, &handle_);
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