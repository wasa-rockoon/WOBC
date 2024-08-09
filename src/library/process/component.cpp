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
  return xTaskCreate(
    entryPoint, name_, stack_size_, this,
    priority_, &task_handle_) == pdPASS;
}

void Component::entryPoint(void* instance) {
  Component *component = static_cast<Component *>(instance);
  component->setup();
  vTaskDelay(1 / portTICK_PERIOD_MS);
  for (;;) {
    while (component->command_listener_.available()) {
      const wcpp::Packet command = component->command_listener_.pop();
      component->onCommand(command);
    }
    component->loop();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}
}