#include "task.h"

namespace process {

Task::Task(const char *name, unsigned stack_size, uint8_t priority, BaseType_t core_id)
  : Process(name), stack_size_(stack_size), priority_(priority), core_id_(core_id) {}

bool Task::onStart() {
#ifdef ESP32
  return xTaskCreatePinnedToCore(
    entryPoint, name_, stack_size_, this,
    priority_, &task_handle_, core_id_) == pdPASS;
#else
  return xTaskCreate(
    entryPoint, name_, stack_size_, this,
    priority_, &task_handle_) == pdPASS;
#endif
}

void Task::entryPoint(void* instance) {
  Task *task = static_cast<Task *>(instance);
  task->setup();
  vTaskDelay(1 / portTICK_PERIOD_MS);
  while (!task->terminated_) {
    task->loop();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

unsigned Task::getMaximumStackUsage() const {
  return stack_size_ - uxTaskGetStackHighWaterMark(task_handle_);
}

}

