#include "process.h"

namespace process {

void Process::startProcess(uint8_t component_id) { 
  component_id_ = component_id;
  bool ok = onStart(); 
  if (!ok) error("pSTF", "process start failed");
}


void Process::listen(kernel::Listener &listener, unsigned queue_size, bool force_push) {
  listener.begin(queue_size, force_push);
  kernel::kernel_.addListener(listener);
}

wcpp::Packet Process::newPacket(uint8_t size) {
  wcpp::Packet packet = kernel::kernel_.allocPacket(size);
  if (packet.isNull()) {
    error("pHOF", "packet heap overflow");
  }
  return packet;
}

wcpp::Packet Process::decodePacket(const uint8_t* buf) {
  uint8_t size = buf[0];
  wcpp::Packet packet = kernel::kernel_.allocPacket(size);
  if (packet.isNull()) {
    error("pHOF", "packet heap overflow");
  }
  else {
    memcpy(packet.getBuf(), buf, size);
  }
  return packet;
}

void Process::sendPacket(const wcpp::Packet &packet) {
  kernel::kernel_.sendPacket(packet);
}
void Process::sendPacket(const wcpp::Packet &packet, const Listener& exclude) {
  kernel::kernel_.sendPacket(packet, &exclude);
}


wcpp::Packet Process::loadPacket(uint8_t packet_id) {
  return kernel::kernel_.loadPacket(packet_id, component_id());
}
bool Process::storePacket(const wcpp::Packet& packet) {
  return kernel::kernel_.storePacket(packet);
}


}