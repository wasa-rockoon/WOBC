#include "component.h"

namespace process {


Component::Component(const char* name, uint8_t id, unsigned command_queue_size, unsigned stack_size)
: Process(name), id_(id), command_queue_size_(command_queue_size), stack_size_(stack_size) {
  component_id_ = id;
  command_listener_.command().component(id);
  memset(store_command_ids, 0x00, WOBC_COMPONENT_STORE_COMMANDS_MAX);
}

bool Component::begin() {
  listen(command_listener_, command_queue_size_, true);
  return onStart();
}

bool Component::storeOnCommand(uint8_t packet_id) {
  for (int i = 0; i < WOBC_COMPONENT_STORE_COMMANDS_MAX; i++) {
    if (store_command_ids[i] == 0x00) {
      store_command_ids[i] = packet_id;
      return true;
    }
  }
  return false;
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
      if (command.dest_unit_id() == unit_id_local || command.dest_unit_id() == kernel::unit_id()) {
        
        // store command
        for (int i = 0; i < WOBC_COMPONENT_STORE_COMMANDS_MAX; i++) {
          if (component->store_command_ids[i] == command.packet_id()) {
            component->storePacket(command);
            break;
          }
        }

      }
      component->onCommand(command);
    }
    component->loop();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}
}