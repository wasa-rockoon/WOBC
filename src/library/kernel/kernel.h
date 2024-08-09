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
#define WOBC_PACKET_HEAP_SIZE 8192
#endif

class Kernel;

extern Kernel kernel_;

class Kernel {
public:
  Kernel();

  wcpp::Packet allocPacket(uint8_t size);
  void sendPacket(const wcpp::Packet& packet, const Listener* exclude = nullptr);

  void addListener(Listener& listener);

  inline void enter() { xSemaphoreTake(mutex_, portMAX_DELAY); }
  inline void exit()  { xSemaphoreGive(mutex_); }

private:
  uint8_t packet_heap_arena_[WOBC_PACKET_HEAP_SIZE];
  Heap packet_heap_;
  PatriciaTrieTree<ListenerArg> packet_listener_tree_;

  SemaphoreHandle_t mutex_;

  unsigned packet_count_;
  unsigned anomaly_count_;


  void refChange(const wcpp::Packet& packet, int change);
  static void refChangeStatic(const wcpp::Packet& packet, int change) { 
    kernel_.refChange(packet, change); 
  };

  friend const unsigned& packetCount();
  friend const unsigned& anomalyCount();
};

inline const unsigned& packetCount() { return kernel_.packet_count_; }
inline const unsigned& anomalyCount() { return kernel_.anomaly_count_; }



}
