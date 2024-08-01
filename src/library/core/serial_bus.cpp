#include "serial_bus.h"
// #include <library/wcpp/cpp/cobs.h>

namespace core {

SerialBus::SerialBus(Stream& serial)
: Task("SerialBus", 4096, 0), serial_(serial) {

};

void SerialBus::begin() {
  startProcess(nullptr);
}

void SerialBus::setup() {
  listen(all_packets, serial_bus_packet_queue_size, true);
}

void SerialBus::loop() {
  // Kernel to serial bus
  {
    const wcpp::Packet packet = all_packets.pop();
    if (packet) {
      serial_.write(packet.encode(), packet.size());
      serial_.write((uint8_t)packet.checksum());
      serial_.write((uint8_t)'\0');
      serial_.flush();
    }
  }

  // Serial bus to kernel
  while (serial_.available() > 0) {
    uint8_t b = serial_.read();
    rx_buf_[rx_count_] = b;
    rx_count_++;

    if (rx_count_ >= wcpp::size_max + 2) { // Too long 
      rx_count_ = 0;
      break;
    }

    if (b == 0) {     
      uint8_t size = rx_buf_[0];
      if (size != rx_count_ - 2) { // Size mismatch
        rx_count_ = 0;
        break;
      }
      uint8_t checksum = rx_buf_[rx_count_ - 2];
      if (checksum != wcpp::Packet::checksum(rx_buf_, rx_count_ - 2)) { // Incorrect checksum
        rx_count_ = 0;
        break;
      }

      wcpp::Packet packet = decodePacket(rx_buf_);

      sendPacket(packet, all_packets);
      rx_count_ = 0;
      break;
    }
  }
}

}
