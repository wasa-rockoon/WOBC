#include "serial_bus.h"
// #include <library/wcpp/cpp/cobs.h>

namespace core {

SerialBus::SerialBus(Stream& serial)
: Task("SerialBus", 128, 0), serial_(serial) {

};

void SerialBus::setup() {
  listen(all_packets, 16, true);
}

void SerialBus::loop() {
  // Kernel to serial bus
  {
    const wcpp::Packet packet = all_packets.pop();
    if (packet) {
      // memcpy(tx_buf_ + 1, packet.encode(), packet.size());
      // tx_buf_[packet.size() + 1] = packet.checksum();
      // unsigned size = COBS::encode(tx_buf_ + 1, packet.size() + 1, tx_buf_);
      // tx_buf_[size] = 0;
      // serial_.write(tx_buf_, size + 1);
      serial_.write(packet.encode(), packet.size());
      serial_.write(byte(0));
    }
  }


  // Serial bus to kernel
  while (serial_.available() > 0) {
    uint8_t b = serial_.read();
    rx_buf_[rx_count_] = b;
    rx_count_++;

    if (rx_count_ + 1 >= wcpp::size_max) { // Too long 
      rx_count_ = 0;
      break;
    }

    if (b == 0) {     
      // unsigned decoded_size = COBS::decode(rx_buf_, rx_count_, rx_buf_);

      uint8_t size = rx_buf_[0];
      if (size != rx_count_ - 1) { // Size mismatch
        rx_count_ = 0;
        break;
      }
      uint8_t checksum = rx_buf_[rx_count_ - 1];
      if (checksum != wcpp::Packet::checksum(rx_buf_, rx_count_ - 1)) { // Incorrect checksum
        rx_count_ = 0;
        break;
      }

      wcpp::Packet packet = decodePacket(rx_buf_);

      send(packet, all_packets);
      rx_count_ = 0;
      break;
    }
  }
}

}
