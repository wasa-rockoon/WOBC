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
Listener& Listener::unit(uint8_t id, uint8_t mask) {
  updateKey(id << 16, mask << 16);
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
    return wcpp::Packet::decode(ptr);
  }
  else {
    return wcpp::Packet::null();
  }
}
const wcpp::Packet Listener::pop() {
  uint8_t* ptr;
  if (xQueueReceive(queue_handle_, &ptr, 0)) {
    return wcpp::Packet::decode(ptr);
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
  if (force_push_) {
    xQueueOverwrite(queue_handle_, &ptr);
    return true;
  }
  else {
    bool pushed = available() > 0;
    xQueueSend(queue_handle_, &ptr, 0);
    return pushed;
  }
}

void Listener::onTraverse(const uint8_t* ptr) {
  if (force_push_) {
    xQueueOverwrite(queue_handle_, &ptr);
  }
  else {
    xQueueSend(queue_handle_, &ptr, 0);
  }
}




}