#include "can_bus.h"
#include "library/driver/can.h"

namespace core {

CANBus::CANBus(pin_t rx, pin_t tx): Task("CANBus", 256), rx_(rx), tx_(tx) {
  instance_ = this;
}

void CANBus::setup() {
  // driver::CAN.begin(can_bps);
  // driver::CAN.setPins(rx_, tx_);
  // driver::CAN.onReceive(CANBus::onReceive_);


  rx_queue_handle_ = xQueueCreate(rx_queue_size, sizeof(FrameQueueItem));
}

void CANBus::loop() {
  // Kernel to CAN bus
  {
    const wcpp::Packet packet = all_packets.pop();
    if (packet) {
      // serial_.write(packet.encode(), packet.size());
      // serial_.write(0);

      int frame = 0;
      int i = 0;

      unsigned id = packet.packet_id() << 21
                  | packet.component_id() << 13
                  | packet.origin_unit_id() << 5;

      // write frames
      while (i < packet.size()) {
        unsigned len = packet.size() - i;
        if (len > 8) len = 8;

        // driver::CAN.beginExtendedPacket(id | frame);
        // if (i == 0) {
        //   driver::CAN.write(packet.encode(), 1);
        //   i = 4;
        //   len -= 1;
        // }
        // driver::CAN.write(packet.encode() + i, len);
        // driver::CAN.endPacket();

        frame++;
        i += len;
      }
    }
  }

  // CAN bus to kernel
  {


  }
}

void CANBus::onReceive(int size) {
  // if (!driver::CAN.packetExtended()) return;
  // if (driver::CAN.packetRtr()) return;

  // uint32_t id = driver::CAN.packetId();

  // uint8_t packet_id      = 0xFF & (id >> 21);
  // uint8_t component_id   = 0xFF & (id >> 13);
  // uint8_t origin_unit_id = 0xFF & (id >> 5);
  // uint8_t frame          = 0xFF & id;

  uint8_t oldest;
  uint32_t oldest_millis;

  // continue frame
  for (int i = 0; i < can_bus_pool_size; i++) {

    // if (pool[i].can_id == id) {
    //   while (driver::CAN.available() > 0) {
    //     // pool[i].data[pool[i].size++] = driver::CAN.read();
    //   }

    //   pool[i].can_id++;
    //   pool[i].received_millis = millis();
    //   return;
    // }

    if (pool[i].can_id == 0) {
      oldest = i;
      break;
    }

    if (oldest_millis - pool[i].received_millis < 0x7FFFFFFF) {
      oldest = i;
      oldest_millis = pool[i].received_millis;
    }
  }

  // first frame

  // if (id & 0xFF != 0) { // missing previous frame
  //   return;
  // }

  // if (pool[oldest].can_id != 0) { // lost frame

  // }

  // pool[oldest].size = 0;
  // pool[oldest].can_id = id + 1;
  // pool[oldest].received_millis = millis();

  // while (driver::CAN.available() > 0) {
  //   // pool[oldest].data[pool[oldest].size++] = driver::CAN.read();
  // }
}

// void CANBus::onReceive(int size) {
//   if (!driver::CAN.packetExtended()) return;
//   if (driver::CAN.packetRtr()) return;

//   FrameQueueItem item;
//   item.can_id = driver::CAN.packetId();
//   while 
//   instance_->rx_queue_handle_
// }

}
