#include "listener.h"

namespace kernel {

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
  return uxQueueMessagesWaiting(queue_handle_);
}

const wcpp::Packet Listener::peek() const {
  uint8_t* ptr;
  if (xQueuePeek(queue_handle_, &ptr, 0)) {
    return wcpp::Packet::decode(ptr, ref_change_);
  }
  else {
    return wcpp::Packet::null();
  }
}
const wcpp::Packet Listener::pop() {
  uint8_t* ptr;
  if (xQueueReceive(queue_handle_, &ptr, 0) == pdPASS) {
    return wcpp::Packet::decode(ptr, ref_change_);
  }
  else {
    return wcpp::Packet::null();
  }
}
unsigned Listener::clear() {
  unsigned cleared = available();
  xQueueReset(queue_handle_);
  return cleared;
}

bool Listener::push(const wcpp::Packet& packet) {
  const uint8_t* ptr = packet.getBuf();
  if (force_push_ && uxQueueSpacesAvailable(queue_handle_) == 0) {
    pop();
  }
  bool pushed = available() > 0;
  xQueueSend(queue_handle_, &ptr, 0);
  return pushed;
}

void Listener::onTraverse(ListenerArg arg) {
  if (arg.exclude == this) return;
  if (force_push_ && uxQueueSpacesAvailable(queue_handle_) == 0) {
    pop();
  }
  xQueueSend(queue_handle_, &arg.packet_buf, 0);
  wcpp::Packet packet = wcpp::Packet::decode(arg.packet_buf);
  ref_change_(packet, +1);
}

key_t Listener::keyOf(const wcpp::Packet& packet) {
  return (key_t)packet.type_and_id() 
       | ((key_t)packet.component_id() << 8)
       | ((key_t)packet.origin_unit_id() << 16)
       | ((key_t)packet.dest_unit_id() << 24);
}


}