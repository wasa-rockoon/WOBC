#pragma once

#include <FreeRTOS.h>
#include <queue.h>
#include "library/wcpp/cpp/packet.h"
#include "patricia_tri_tree.h"

namespace kernel {

class Listener: public kernel::PatriciaTrieTree<const uint8_t*>::Node {
public:
  Listener();
  ~Listener();

  void begin(unsigned queue_size = 1, bool force_push = true);

  // Set filter
  Listener& command();
  Listener& telemetry();
  Listener& packet(uint8_t id, uint8_t mask = 0xFF);
  Listener& component(uint8_t id, uint8_t mask = 0xFF);
  Listener& unit(uint8_t id, uint8_t mask = 0xFF);

  unsigned available() const;
  inline operator bool() const { return available() == 0; }
  inline bool operator!() const { return available() != 0; }

  const wcpp::Packet peek() const;
  const wcpp::Packet pop();
  unsigned clear();
  
  bool push(const wcpp::Packet& packet);

private:
  QueueHandle_t queue_handle_;
  bool force_push_;

  void onTraverse(const uint8_t* ptr) override;

  Listener& updateKey(key_t key, key_t mask);
};


}