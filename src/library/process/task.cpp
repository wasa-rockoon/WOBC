#include "task.h"

namespace process {

Task::Task(const char *name, unsigned stack_size, uint8_t priority)
  : Process(name), stack_size_(stack_size), priority_(priority) {}

void Task::onStart() {
  xTaskCreate(
    entryPoint, name_, stack_size_ * sizeof(size_t), this,
    priority_, &task_handle_);
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

