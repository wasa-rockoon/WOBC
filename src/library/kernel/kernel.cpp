#include "kernel.h"

namespace kernel {
  
Kernel::Kernel()
: packet_heap_(packet_heap_arena_, WOBC_PACKET_HEAP_SIZE), 
  mutex_(xSemaphoreCreateMutex()) {

}

void Kernel::addListener(Listener& listener) {
  listener.begin();
  enter();
  packet_listener_tree_.insert(listener); 
  exit();
}

}