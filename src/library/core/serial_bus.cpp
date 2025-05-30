#include "serial_bus.h"

namespace core {

SerialBus::SerialBus(Stream& serial)
: CoreTask("SerialBus", WOBC_SERIAL_BUS_STACK_SIZE, WOBC_SERIAL_BUS_PRIORITY), serial_(serial) {

};


void SerialBus::setup() {
  listen(all_packets, WOBC_SERIAL_BUS_PACKET_QUEUE_SIZE, true);
}

void SerialBus::loop() {
  // Kernel to serial bus
  {
    while (all_packets.available()) {
      const wcpp::Packet packet = all_packets.pop();
      const uint8_t* buf = packet.encode();
      // portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
      // taskENTER_CRITICAL(&mux);
      serial_.write(buf, packet.size());
      serial_.write((uint8_t)packet.checksum());
      serial_.write((uint8_t)'\0');
      // taskEXIT_CRITICAL(&mux);
    }
  }

  // Serial bus to kernel
  while (serial_.available() > 0) {
    uint8_t b = serial_.read();
    rx_buf_[rx_count_] = b;
    rx_count_++;

    if (rx_count_ >= wcpp::size_max + 2) {
      error("sbRL", "serial bus, received too long: %d", rx_count_);
      rx_count_ = 0;
      break;
    }

    if (b == 0) {     
      uint8_t size = rx_buf_[0];
      if (size > rx_count_ - 2) continue;
      if (size != rx_count_ - 2) {
        error("sbSM", "serial bus, size mismatch. expected: %d, actual: %d", size, rx_count_ - 2);
        rx_count_ = 0;
        break;
      }
      uint8_t checksum = rx_buf_[rx_count_ - 2];
      uint8_t checksum_calc = wcpp::Packet::checksum(rx_buf_, rx_count_ - 2);
      if (checksum != checksum_calc) { // Incorrect checksum
        error("sbCS", "serial bus, incorrect checksum. expected: %d, actual: %d", checksum, checksum_calc);
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
