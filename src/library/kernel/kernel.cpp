#include "kernel.h"

namespace kernel {

Kernel kernel_;

Kernel::Kernel()
: packet_heap_(packet_heap_arena_, WOBC_PACKET_HEAP_SIZE), 
  mutex_(xSemaphoreCreateMutex()) {
}

wcpp::Packet Kernel::allocPacket(uint8_t size) {
  enter();
  uint8_t* buf = static_cast<uint8_t*>(packet_heap_.alloc(size));
  exit();
  if (buf == nullptr) return wcpp::Packet::null();
  memset(buf, 0, size);
  return wcpp::Packet::empty(buf, size, refChangeStatic);
}


void Kernel::refChange(wcpp::Packet& packet, int change) {
  enter();
  while (change > 0) {
    packet_heap_.addRef(static_cast<void*>(packet.getBuf()));
    change--;
  }
  while (change < 0) {
    packet_heap_.releaseRef(static_cast<void*>(packet.getBuf()));
    change++;
  }
  exit();
}

void Kernel::sendPacket(const wcpp::Packet& packet, const Listener* exclude) {
  const uint8_t* buf = packet.getBuf();
  if (packet_heap_.inHeap(packet.getBuf())) {
    ListenerArg arg = {packet.getBuf(), exclude};
    packet_listener_tree_.traverse(Listener::keyOf(packet), arg);
  }
  else {
    wcpp::Packet packet_copied = allocPacket(packet.size());
    packet_copied = packet;
    ListenerArg arg = {packet_copied.getBuf(), exclude};
    packet_listener_tree_.traverse(Listener::keyOf(packet_copied), arg);
  }
}

void Kernel::addListener(Listener& listener) {
  listener.setRefChange(refChangeStatic);
  enter();
  packet_listener_tree_.insert(listener); 
  exit();
}

}

