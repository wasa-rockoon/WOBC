#pragma once

#include "library/common.h"
#include "library/process/task.h"
#include "library/driver/can.h"

namespace core {

#ifndef WOBC_CAN_BUS_BAUDRATE
#define WOBC_CAN_BUS_BAUDRATE 1000000
#endif
#ifndef WOBC_CAN_BUS_POOL_SIZE
#define WOBC_CAN_BUS_POOL_SIZE 4
#endif
#ifndef WOBC_CAN_BUS_RX_QUEUE_SIZE
#define WOBC_CAN_BUS_RX_QUEUE_SIZE 32
#endif
#ifndef WOBC_CAN_BUS_STACK_SIZE
#define WOBC_CAN_BUS_STACK_SIZE 4048
#endif
#ifndef WOBC_CAN_BUS_PACKET_QUEUE_SIZE
#define WOBC_CAN_BUS_PACKET_QUEUE_SIZE 16
#endif
#ifndef WOBC_CAN_BUS_PRIORITY
#define WOBC_CAN_BUS_PRIORITY 7
#endif

class CANBus: public process::CoreTask, public driver::CAN::Receiver {
public:
  CANBus(pin_t rx, pin_t tx);

private:
  driver::CAN can_;
  pin_t rx_;
  pin_t tx_;
  Listener all_packets;

  struct PacketPoolSlot {
    uint32_t can_id;
    uint32_t received_millis;
    uint8_t size;
    wcpp::Packet packet;

    PacketPoolSlot(): can_id(0), received_millis(0), size(0), packet(wcpp::Packet::null()) {}
  };
  PacketPoolSlot pool_[WOBC_CAN_BUS_POOL_SIZE];

  struct FrameQueueItem {
    uint32_t can_id;
    uint8_t length;
    uint8_t data[8];
  };
  
  QueueHandle_t rx_queue_handle_;

  void setup() override;
  void loop() override;

  void onReceive(const driver::CAN::Frame& frame) override;
  void onError() override;
};

}
