#pragma once

#include "library/common.h"
#include <queue.h>
#include <semphr.h>
#include "library/wcpp/cpp/packet.h"
#include "heap.h"
#include "patricia_tri_tree.h"
#include "listener.h"

namespace kernel {

#ifndef WOBC_PACKET_HEAP_SIZE
#define WOBC_PACKET_HEAP_SIZE 32768
#endif


constexpr unsigned send_packet_queue_size = 8;

class Kernel;

extern Kernel kernel_;

class Kernel {
public:
  Kernel();

  wcpp::Packet allocPacket(uint8_t size);
  void sendPacket(const wcpp::Packet& packet, const Listener* exclude = nullptr);

  void addListener(Listener& listener);

private:
  uint8_t packet_heap_arena_[WOBC_PACKET_HEAP_SIZE];
  Heap packet_heap_;
  PatriciaTrieTree<ListenerArg> packet_listener_tree_;

  QueueHandle_t packet_queue_handle_;
  SemaphoreHandle_t mutex_;

  inline void enter() { xSemaphoreTake(mutex_, portMAX_DELAY); }
  inline void exit()  { xSemaphoreGive(mutex_); }

  void refChange(wcpp::Packet& packet, int change);
  static void refChangeStatic(wcpp::Packet& packet, int change) { 
    kernel_.refChange(packet, change); 
    };
};


}
