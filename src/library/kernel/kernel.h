#pragma once

#include "library/common.h"
#include <queue.h>
#include <semphr.h>
#include "library/wcpp/cpp/packet.h"
#include "heap.h"
#include "patricia_tri_tree.h"
#include "listener.h"
#include "library/driver/driver.h"

namespace kernel {

#ifndef WOBC_PACKET_HEAP_SIZE
#define WOBC_PACKET_HEAP_SIZE 16384
#endif

class Kernel;

extern Kernel kernel_;

class Kernel {
public:
  Kernel();

  bool begin(uint8_t module_id, bool overwrite_module_id = false);

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

  driver::KVS kvs_;
  static const driver::KVS::key_t module_id_kvs_key = 0xFFFF;

  SemaphoreHandle_t mutex_;
  xTimerHandle timer_handle_;

  uint8_t module_id_ = 0xFF;
  uint8_t unit_id_ = 0xFF;

  void refChange(const wcpp::Packet& packet, int change);
  static void refChangeStatic(const wcpp::Packet& packet, int change) { 
    kernel_.refChange(packet, change); 
  };

  static void timerEntryPoint(xTimerHandle timer_handle);

  friend const unsigned& packetCount();
  friend const unsigned& errorCount();
  friend void setUnitId(uint8_t unit_id);
  friend uint8_t module_id();
  friend uint8_t unit_id();
};

// System calls

inline bool begin(uint8_t module_id, bool overwrite_module_id = false) { 
  return kernel_.begin(module_id, overwrite_module_id); 
  }

inline void setUnitId(uint8_t unit_id) { kernel_.unit_id_ = unit_id; }

inline const unsigned& packetCount() { return kernel_.packet_count_; }
inline const unsigned& errorCount() { return kernel_.error_count_; }

inline uint8_t module_id() { return kernel_.module_id_; }
inline uint8_t unit_id() { return kernel_.unit_id_; }

}
