#include "can_monitor.h"

namespace component {

CANMonitor::CANMonitor()
  : process::Component("CANMonitor", component_id) {
}

void CANMonitor::setup() {
  listen(can_packets_, 8);
}

void CANMonitor::loop() {
  while (can_packets_) {
    const wcpp::Packet packet = can_packets_.pop();
    if (!packet) continue;

    const uint8_t* buf = packet.encode();
    //LOG("CAN RX pkid:%02X comp:%02X unit:%02X size:%d",
        //packet.packet_id(), packet.component_id(), packet.origin_unit_id(), packet.size());

    int payload_size = packet.size() - 3;
    if (payload_size > 8) payload_size = 8;
    if (payload_size > 0) {
      char hex[3 * 8 + 1];
      int idx = 0;
      for (int i = 0; i < payload_size; i++) {
        idx += snprintf(hex + idx, sizeof(hex) - idx, "%02X", buf[4 + i]);
      }
      hex[idx] = '\0';
      //LOG("CAN RX data: %s", hex);
    }
  }
}

}