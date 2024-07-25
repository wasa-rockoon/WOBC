#pragma once

#include <FreeRTOS.h>
#include <queue.h>
#include "library/wcpp/cpp/packet.h"
#include "patricia_tri_tree.h"

namespace kernel {

class Listener;

struct ListenerArg {
  const uint8_t* packet_buf;
  const Listener* exclude;
};

class Listener: public kernel::PatriciaTrieTree<ListenerArg>::Node {
public:
  Listener() {};
  // ~Listener();

  void begin(unsigned queue_size = 1, bool force_push = true);

  // Set filter
  Listener& command();
  Listener& telemetry();
  Listener& packet(uint8_t id, uint8_t mask = 0xFF);
  Listener& component(uint8_t id, uint8_t mask = 0xFF);
  Listener& unit_origin(uint8_t id, uint8_t mask = 0xFF);
  Listener& unit_dest(uint8_t id, uint8_t mask = 0xFF);

  unsigned available() const;
  inline operator bool() const { return available() == 0; }
  inline bool operator!() const { return available() != 0; }

  const wcpp::Packet peek() const;
  const wcpp::Packet pop();
  unsigned clear();
  
  bool push(const wcpp::Packet& packet);

  inline void setRefChange(wcpp::Packet::ref_change_t ref_change) { ref_change_ = ref_change; } 
  static key_t keyOf(const wcpp::Packet& packet);

private:
  wcpp::Packet::ref_change_t ref_change_;
  QueueHandle_t queue_handle_;
  bool force_push_;

  void onTraverse(ListenerArg arg) override;

  Listener& updateKey(key_t key, key_t mask);
};


}