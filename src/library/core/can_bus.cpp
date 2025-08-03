#include "can_bus.h"

namespace core {

CANBus::CANBus(pin_t rx, pin_t tx): CoreTask("CANBus", WOBC_CAN_BUS_STACK_SIZE, WOBC_CAN_BUS_PRIORITY), can_(*this), rx_(rx), tx_(tx) {
}

void CANBus::setup() {
  for (int i = 0; i < WOBC_CAN_BUS_POOL_SIZE; i++) {
    pool_[i].size = 0;
    pool_[i].can_id = 0;
    pool_[i].received_millis = 0;
    pool_[i].packet = wcpp::Packet::null();
  }

  can_.begin(WOBC_CAN_BUS_BAUDRATE, rx_, tx_);

  rx_queue_handle_ = xQueueCreate(WOBC_CAN_BUS_RX_QUEUE_SIZE, sizeof(FrameQueueItem));
  listen(all_packets, WOBC_CAN_BUS_PACKET_QUEUE_SIZE, true);
}

void CANBus::loop() {
  // Kernel to CAN bus
  {
    const wcpp::Packet packet = all_packets.pop();
    if (packet && packet.size() >= 4) {

      uint32_t id = (uint32_t)packet.packet_id() << 21
                  | (uint32_t)packet.component_id() << 13
                  | (uint32_t)packet.origin_unit_id() << 5;

      const uint8_t* buf = packet.encode();

      driver::CAN::Frame frame;
      frame.id = id;
      frame.extended = true;
      frame.rtr      = false;

      int i = 0;

      // write frames
      while (i < packet.size()) {

        if (i == 0) { // first frame
          frame.length = packet.size() - 3;
          if (frame.length > 8) frame.length = 8;

          frame.data[0] = packet.size();
          std::memcpy(frame.data + 1, buf + 4, frame.length - 1);

          i = 4 + frame.length - 1;
        }
        else {
          frame.length = packet.size() - i;
          if (frame.length > 8) frame.length = 8;

          std::memcpy(frame.data, buf + i, frame.length);

          i += frame.length;
        }

        // Serial.printf("send %d %d %s\n", frame.id, frame.length, frame.data);

        if (!can_.send(frame)) {
          error_(all_packets, "cbSN", "CAN bus, send error");
          break;
        }
        frame.id++;
      }
    }
  }

  can_.update();

  // CAN bus to kernel
  {
    FrameQueueItem item;
    if (xQueueReceive(rx_queue_handle_, &item, 0) == pdPASS) {
      uint8_t oldest = 0;
      uint32_t oldest_millis = 0;

      // printf("receive %d %d %s\n", item.can_id, item.length, item.data);

      // continue frame
      for (int i = 0; i < WOBC_CAN_BUS_POOL_SIZE; i++) {

        if (pool_[i].can_id == item.can_id) {
          // Serial.printf("P %d %d %d %d %d %d\n", item.can_id, i, pool_[i].size, pool_[i].packet.size(), item.length,  kernel::kernel_.packet_heap_.getRefCount(pool_[i].packet.getBuf()));
          memcpy(pool_[i].packet.getBuf() + pool_[i].size, item.data, item.length);
          pool_[i].size += item.length;
          pool_[i].can_id++;
          pool_[i].received_millis = millis();

          if (pool_[i].size == pool_[i].packet.size()) { // complete frame
            // Serial.printf("LAST %d %d\n", pool_[i].size, kernel::kernel_.packet_heap_.getRefCount(pool_[i].packet.getBuf()));
            sendPacket(pool_[i].packet, all_packets);
            pool_[i].packet.clear();
            pool_[i].can_id = 0;
          }
          else if (pool_[i].size > pool_[i].packet.size()) { // wrong size
            error_(all_packets, "cbWS", "CAN bus, wrong size, expected: %d, actual: %d", pool_[i].size, pool_[i].packet.size());
            // printf("WRONG SIZE %d %d\n", pool_[i].size, pool_[i].packet.size());
            pool_[i].packet.clear();
            pool_[i].can_id = 0;
          }

          return;
        }

        // find oldest slot
        if (pool_[i].can_id == 0) {
          oldest = i;
          oldest_millis = 0;
        }
        if (oldest_millis - pool_[i].received_millis < 0x7FFFFFFF) {
          oldest = i;
          oldest_millis = pool_[i].received_millis;
        }
      }

      //first frame

      if ((item.can_id & 0xFF) != 0) { // missing previous frame
        error_(all_packets, "cbDF", "CAN bus, drop %dth frame", item.can_id & 0xFF);
        return;
      }

      if (pool_[oldest].can_id != 0) { // lost frame
        error_(all_packets, "cbLF", "CAN bus, lost frame, id:%X %X %x, %d", 
              0xFF & (item.can_id >> 21), 0xFF & (item.can_id >> 13), 0xFF & (item.can_id >> 5), 
              32 & pool_[oldest].can_id, oldest);
        pool_[oldest].can_id = 0;
      }

      if (item.length < 1) {
        error_(all_packets, "cbEF", "CAN bus, empty frame");
        return;
      }

      uint8_t packet_id      = 0xFF & (item.can_id >> 21);
      uint8_t component_id   = 0xFF & (item.can_id >> 13);
      uint8_t origin_unit_id = 0xFF & (item.can_id >> 5);
      // uint8_t frame          = 0xFF & item.can_id;
      pool_[oldest].packet = newPacket(item.data[0]);
      if (!pool_[oldest].packet) return;
      // Serial.printf("A %d %d\n", pool_[oldest].packet.getBuf() - kernel::kernel_.packet_heap_arena_, kernel::kernel_.packet_heap_.getRefCount(pool_[oldest].packet.getBuf()));
      uint8_t* buf = pool_[oldest].packet.getBuf();
      buf[0] = item.data[0];
      buf[1] = packet_id;
      buf[2] = component_id;
      buf[3] = origin_unit_id;
      memcpy(buf + 4, item.data + 1, item.length - 1); 

      if (pool_[oldest].packet.size() <= 10) { // single frame
          if (pool_[oldest].packet.size() != item.length + 3) { // wrong size
            error_(all_packets, "cbWS", "CAN bus, wrong size first, expected: %d, actual: %d", 
                   pool_[oldest].size, item.length + 3);
            pool_[oldest].packet.clear();
            pool_[oldest].can_id = 0;
          }

        sendPacket(pool_[oldest].packet, all_packets);
        pool_[oldest].packet.clear();
        pool_[oldest].can_id = 0;
      }
      else { // multiple frames
        pool_[oldest].size = 3 + item.length;
        pool_[oldest].can_id = item.can_id + 1;
        pool_[oldest].received_millis = millis();
      }
    }
  }
}

void CANBus::onReceive(const driver::CAN::Frame& frame) {
  // printf("CAN\n");
  if (!frame.extended) return;
  if (frame.rtr) return;

  FrameQueueItem item;
  item.can_id = frame.id;
  item.length = frame.length;
  memcpy(item.data, frame.data, frame.length);

  if (xQueueSendFromISR(rx_queue_handle_, &item, 0) != pdPASS) {

  }
}

void CANBus::onError() {
  error_(all_packets, "cbER", "CAN bus, can error");
}

}
