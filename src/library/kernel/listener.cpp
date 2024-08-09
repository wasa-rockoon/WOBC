#include "listener.h"

namespace kernel {


Listener::Listener(): queue_handle_(nullptr) {} 

void Listener::begin(unsigned queue_size, bool force_push) {
  queue_handle_ = xQueueCreate(queue_size, sizeof(uint8_t*));
  force_push_ = force_push;
}

Listener& Listener::command() {
  updateKey(0, wcpp::packet_type_mask);
  return *this;
}
Listener& Listener::telemetry() {
  updateKey(wcpp::packet_type_mask, wcpp::packet_type_mask);
  return *this;
}
Listener& Listener::packet(uint8_t id, uint8_t mask) {
  updateKey(id, wcpp::packet_id_mask & mask);
  return *this;
}
Listener& Listener::component(uint8_t id, uint8_t mask) {
  updateKey(id << 8, mask << 8);
  return *this;
}
Listener& Listener::unit_origin(uint8_t id, uint8_t mask) {
  updateKey(id << 16, mask << 16);
  return *this;
}
Listener& Listener::unit_dest(uint8_t id, uint8_t mask) {
  updateKey(id << 24, mask << 24);
  return *this;
}

Listener& Listener::updateKey(key_t key, key_t mask) {
  setKey((key & mask) | (getKey() & ~mask), mask | getMask());
  return *this;
}

unsigned Listener::available() const {
  if (queue_handle_ == nullptr) return 0;
  return uxQueueMessagesWaiting(queue_handle_);
}

const wcpp::Packet Listener::peek() const {
  uint8_t* ptr;
  if (queue_handle_ != nullptr && xQueuePeek(queue_handle_, &ptr, 0)) {
    wcpp::Packet packet = wcpp::Packet::decode(ptr, ref_change_);
    ref_change_(packet, +1);
    return packet;
  }
  else {
    return wcpp::Packet::null();
  }
}
const wcpp::Packet Listener::pop() {
  uint8_t* ptr;
  if (queue_handle_ != nullptr && xQueueReceive(queue_handle_, &ptr, 0) == pdPASS) {
    wcpp::Packet packet = wcpp::Packet::decode(ptr, ref_change_);
    return packet;
  }
  else {
    return wcpp::Packet::null();
  }
}
unsigned Listener::clear() {
  if (queue_handle_ == nullptr) return 0;
  unsigned cleared = available();
  xQueueReset(queue_handle_);
  return cleared;
}

bool Listener::push(const wcpp::Packet& packet) {
  if (queue_handle_ == nullptr) return false;
  const uint8_t* ptr = packet.getBuf();
  if (force_push_ && uxQueueSpacesAvailable(queue_handle_) == 0) {
    pop();
  }
  ref_change_(packet, +1);
  if (xQueueSend(queue_handle_, &ptr, 0) == pdPASS) {
    return true;
  }
  return false;
}

void Listener::onTraverse(ListenerArg arg) {
  if (arg.exclude == this) return;

  wcpp::Packet packet = wcpp::Packet::decode(arg.packet_buf, ref_change_);
  ref_change_(packet, +1);
  push(packet);
}

key_t Listener::keyOf(const wcpp::Packet& packet) {
  return (key_t)packet.type_and_id() 
       | ((key_t)packet.component_id() << 8)
       | ((key_t)packet.origin_unit_id() << 16)
       | ((key_t)packet.dest_unit_id() << 24);
}


}