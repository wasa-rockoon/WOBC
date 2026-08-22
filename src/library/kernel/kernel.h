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

// Number of independently sequenced packet streams tracked by the kernel.
#ifndef WOBC_PACKET_SEQUENCE_STREAMS
#define WOBC_PACKET_SEQUENCE_STREAMS 32
#endif

class Kernel;

extern Kernel kernel_;

class Kernel {
public:
  Kernel();

  bool begin(uint8_t module_id, bool check_module_id = true);

  wcpp::Packet allocPacket(uint8_t size);
  uint16_t nextPacketSequence(uint8_t origin_unit_id, uint8_t dest_unit_id,
                              uint8_t component_id, uint8_t type_and_id);
  void sendPacket(const wcpp::Packet& packet, const Listener* exclude = nullptr);

  void addListener(Listener& listener);

  bool storePacket(const wcpp::Packet& packet);
  wcpp::Packet loadPacket(uint8_t packet_id, uint8_t component_id);

  inline void enter() { xSemaphoreTake(mutex_, portMAX_DELAY); }
  inline void exit()  { xSemaphoreGive(mutex_); }

private:
  uint8_t packet_heap_arena_[WOBC_PACKET_HEAP_SIZE];
  Heap packet_heap_;
  PatriciaTrieTree<ListenerArg> packet_listener_tree_;
  unsigned packet_count_;
  struct PacketSequenceStream {
    uint32_t key = 0;
    uint16_t next = 0;
    bool used = false;
  };
  PacketSequenceStream packet_sequence_streams_[WOBC_PACKET_SEQUENCE_STREAMS];

  unsigned error_count_;

  driver::KVS kvs_;
  static const driver::KVS::key_t module_id_kvs_key = 0xFFFF;

  SemaphoreHandle_t mutex_;
  xTimerHandle timer_handle_;

  uint8_t module_id_ = 0xFF;
  uint8_t unit_id_ = 0xFF;
  bool unit_id_set_ = false;

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

inline bool begin(uint8_t module_id, bool check_module_id = true) { 
  return kernel_.begin(module_id, check_module_id); 
  }

inline void setUnitId(uint8_t unit_id) { 
  kernel_.unit_id_ = unit_id; 
  kernel_.unit_id_set_ = true;
}

inline const unsigned& packetCount() { return kernel_.packet_count_; }
inline const unsigned& errorCount() { return kernel_.error_count_; }

// Returns the next sequence number for this origin/destination/component/packet
// type stream. The 16-bit value wraps naturally after 65535.
inline uint16_t nextPacketSequence(uint8_t origin_unit_id, uint8_t dest_unit_id,
                                   uint8_t component_id, uint8_t type_and_id) {
  return kernel_.nextPacketSequence(origin_unit_id, dest_unit_id, component_id, type_and_id);
}

inline uint8_t module_id() { return kernel_.module_id_; }
inline uint8_t unit_id() { return kernel_.unit_id_; }

}
