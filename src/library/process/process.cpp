#include "process.h"

namespace process {

void Process::startProcess(Component* component) { 
  component_ = component;
  onStart(); 
}


void Process::listen(kernel::Listener &listener, unsigned queue_size, bool force_push) {
  listener.begin(queue_size, force_push);
  kernel::kernel_.addListener(listener);
}

wcpp::Packet Process::newPacket(uint8_t size) {
  return kernel::kernel_.allocPacket(size);
}

wcpp::Packet Process::decodePacket(const uint8_t* buf) {
  uint8_t size = buf[0];
  wcpp::Packet packet = kernel::kernel_.allocPacket(size);
  memcpy(packet.getBuf(), buf, size);
  return packet;
}

void Process::sendPacket(const wcpp::Packet &packet) {
  kernel::kernel_.sendPacket(packet);
}
void Process::sendPacket(const wcpp::Packet &packet, const Listener& exclude) {
  kernel::kernel_.sendPacket(packet, &exclude);
}

}