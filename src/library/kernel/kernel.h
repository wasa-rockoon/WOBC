#pragma once

#include <FreeRTOS.h>
#include <semphr.h>
#include "library/wcpp/cpp/packet.h"
#include "heap.h"
#include "patricia_tri_tree.h"
#include "listener.h"

namespace kernel {

#ifndef WOBC_PACKET_HEAP_SIZE
#define WOBC_PACKET_HEAP_SIZE 32768
#endif

class Kernel {
public:
  Kernel();

  void begin();

  wcpp::Packet allocPacket(uint8_t size);
  bool sendPacket(const wcpp::Packet& packet);

  void addListener(Listener& listener);

private:
  uint8_t packet_heap_arena_[WOBC_PACKET_HEAP_SIZE];
  kernel::Heap packet_heap_;
  kernel::PatriciaTrieTree<const uint8_t*> packet_listener_tree_;

  SemaphoreHandle_t mutex_;

  inline void enter() { xSemaphoreTake(mutex_, portMAX_DELAY); }
  inline void exit()  { xSemaphoreGive(mutex_); }
};

}
