#include "task.h"

namespace process {

Task::Task(const char *name, unsigned stack_size, uint8_t priority)
  : Process(name), stack_size_(stack_size), priority_(priority) {}

bool Task::onStart() {
  return xTaskCreatePinnedToCore(
    entryPoint, name_, stack_size_, this,
    priority_, &task_handle_, 1) == pdPASS;
}

void Task::entryPoint(void* instance) {
  Task *task = static_cast<Task *>(instance);
  task->setup();
  taskYIELD();
  while (!task->terminated_) {
    task->loop();
    taskYIELD();
  }
}


}

