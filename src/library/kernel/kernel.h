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

#ifndef WOBC_CRASH_ERROR_LEVEL
#define WOBC_CRASH_ERROR_LEVEL 1000
#endif

#ifndef WOBC_ERROR_SUMMARY_SIZE
#define WOBC_ERROR_SUMMARY_SIZE 8
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
  unsigned packet_count_;

  unsigned error_count_;

  SemaphoreHandle_t mutex_;

  uint8_t module_id_ = 0xFF;
  uint8_t unit_id_ = 0xFF;

  void refChange(const wcpp::Packet& packet, int change);
  static void refChangeStatic(const wcpp::Packet& packet, int change) { 
    kernel_.refChange(packet, change); 
  };

  friend const unsigned& packetCount();
  friend const unsigned& errorCount();
  friend uint8_t module_id();
  friend uint8_t unit_id();
  friend void set_module_id(uint8_t id);
  friend void set_unit_id(uint8_t id);
};

// System calls

void begin();

inline const unsigned& packetCount() { return kernel_.packet_count_; }
inline const unsigned& errorCount() { return kernel_.error_count_; }

inline uint8_t module_id() { return kernel_.module_id_; }
inline uint8_t unit_id() { return kernel_.unit_id_; }

inline void set_module_id(uint8_t id) { kernel_.module_id_ = id; }
inline void set_unit_id(uint8_t id) { kernel_.unit_id_ = id; }

const wcpp::Packet errorSummary();


}
