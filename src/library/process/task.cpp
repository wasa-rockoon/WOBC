#include "task.h"

namespace process {

Task::Task(const char *name, unsigned stack_size, uint8_t priority)
  : Process(name), stack_size_(stack_size), priority_(priority) {}

bool Task::onStart() {
  return xTaskCreate(
    entryPoint, name_, stack_size_, this,
    priority_, &task_handle_) == pdPASS;
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

