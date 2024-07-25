#pragma once

#include "library/common.h"
#include "library/process/task.h"

namespace core {

const unsigned can_bps = 1E6;
const unsigned can_bus_pool_size = 8;
const unsigned rx_queue_size = 32;

class CANBus: public process::Task {
public:
  CANBus(pin_t rx, pin_t tx);

private:
  pin_t rx_;
  pin_t tx_;
  Listener all_packets;

  struct PacketPoolSlot {
    uint32_t can_id;
    uint32_t received_millis;
    uint8_t size;
    wcpp::Packet packet;

    PacketPoolSlot(): can_id(0), received_millis(0), size(0), packet(wcpp::Packet::null()) {}
    // uint8_t data[wcpp::size_max];
  };
  PacketPoolSlot pool[can_bus_pool_size];

  struct FrameQueueItem {
    uint32_t can_id;
    uint8_t can_dlc;
    uint8_t data[8];
  };
  
  QueueHandle_t rx_queue_handle_;

  void setup() override;
  void loop() override;

  void onReceive(int size);
  static void onReceive_(int size) { instance_->onReceive(size); };

  static CANBus* instance_;

};

}
