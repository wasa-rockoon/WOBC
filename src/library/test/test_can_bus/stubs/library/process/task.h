#pragma once

#include "library/common.h"
#include "library/wcpp/cpp/packet.h"
#include <memory>

namespace process {

// Only the task boundary is faked; CANBus itself and WCPP use production code.
class CoreTask {
public:
  class Listener {
  public:
    std::deque<wcpp::Packet> packets;
    wcpp::Packet pop() {
      if (packets.empty()) return wcpp::Packet::null();
      wcpp::Packet packet = packets.front();
      packets.pop_front();
      return packet;
    }
  };

  CoreTask(const char*, unsigned, unsigned) {}
  virtual ~CoreTask() = default;
  void begin() { setup(); }
  void step() { loop(); }
  void enqueue(const wcpp::Packet& packet) { listener_->packets.push_back(packet); }
  std::vector<std::vector<uint8_t>> delivered;
  unsigned errors = 0;

protected:
  virtual void setup() = 0;
  virtual void loop() = 0;
  void listen(Listener& listener, unsigned, bool) { listener_ = &listener; }
  wcpp::Packet newPacket(uint8_t size) {
    buffers_.emplace_back(new uint8_t[size]{});
    return wcpp::Packet::empty(buffers_.back().get(), size);
  }
  void sendPacket(const wcpp::Packet& packet, const Listener& exclude) {
    // The incoming packet must not be echoed back onto its source CAN bus.
    if (&exclude != listener_) ++errors;
    delivered.emplace_back(packet.encode(), packet.encode() + packet.size());
  }
  template <typename... Args>
  void error_(const Listener&, const char*, const char*, Args...) { ++errors; }

private:
  Listener* listener_ = nullptr;
  std::vector<std::unique_ptr<uint8_t[]>> buffers_;
};

} // namespace process
