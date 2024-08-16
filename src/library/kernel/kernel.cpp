#include "kernel.h"

namespace kernel {

Kernel kernel_;

Kernel::Kernel()
: packet_heap_(packet_heap_arena_, WOBC_PACKET_HEAP_SIZE), 
  mutex_(xSemaphoreCreateMutex()) {
}

bool Kernel::begin(uint8_t module_id, bool check_module_id) {
  if (!kvs_.begin()) return false;

  // check module id
  if (kvs_.exists(module_id_kvs_key) && kvs_.sizeOf(module_id_kvs_key) == 1) {
    uint8_t module_id_stored;
    kvs_.read(module_id_kvs_key, &module_id_stored);

    if (check_module_id) {
      assert(module_id == module_id_stored);
    }
    else {
      kvs_.clear(module_id_kvs_key);
    }
  }
  else {
    if (check_module_id) kvs_.write(module_id_kvs_key, &module_id, 1);
  }

  kernel_.module_id_ = module_id;
  timer_handle_ = xTimerCreate(
    "Kernel", 1000, pdTRUE, this, timerEntryPoint);

  return xTimerStart(timer_handle_, 0) == pdPASS;
}

void Kernel::timerEntryPoint(TimerHandle_t timer_handle) {
  wcpp::Packet heartbeat = kernel_.allocPacket(32);
  heartbeat.telemetry(packet_id_heartbeat, module_id());
  heartbeat.append("Im").setInt(module_id());
  if (kernel_.unit_id_set_) {
    heartbeat.append("Iu").setInt(unit_id());
  }
  kernel_.sendPacket(heartbeat);
}

wcpp::Packet Kernel::allocPacket(uint8_t size) {
  enter();
  uint8_t* buf = static_cast<uint8_t*>(packet_heap_.alloc(size));
  exit();

  if (buf == nullptr) {
    return wcpp::Packet::null(); 
  }
  
  memset(buf, 0, size);
  return wcpp::Packet::empty(buf, size, refChangeStatic);
}


void Kernel::refChange(const wcpp::Packet& packet, int change) {
  // Serial.printf("change %d %d %d\n", packet.getBuf() - packet_heap_arena_, change, packet_heap_.getRefCount(packet.getBuf()));
  enter();
  while (change > 0) {
    packet_heap_.addRef(static_cast<const void*>(packet.getBuf()));
    change--;
  }
  while (change < 0) {
    packet_heap_.releaseRef(static_cast<const void*>(packet.getBuf()));
    change++;
  }
  exit();
}

void Kernel::sendPacket(const wcpp::Packet& packet, const Listener* exclude) {

  if (packet.packet_id() == packet_id_heartbeat && packet.origin_unit_id() == unit_id_local) {
    auto e = packet.find("Iu");
    if (e) {
      if (unit_id_set_) {
        assert((*e).getInt() == unit_id());
      }
      else {
        unit_id_ = (*e).getInt(); 
      }
    }
  }

  if (packet.packet_id() == packet_id_error) {
    error_count_++;
  } 

  if (packet_heap_.inHeap(packet.getBuf())) {
    ListenerArg arg = {packet.getBuf(), exclude};
    packet_listener_tree_.traverse(Listener::keyOf(packet), arg);
  }
  else {
    wcpp::Packet packet_copied = allocPacket(packet.size());
    packet_copied.copy(packet);
    ListenerArg arg = {packet_copied.getBuf(), exclude};
    packet_listener_tree_.traverse(Listener::keyOf(packet_copied), arg);
  }
  packet_count_++;
}

void Kernel::addListener(Listener& listener) {
  listener.setRefChange(refChangeStatic);
  enter();
  packet_listener_tree_.insert(listener); 
  exit();
}


bool Kernel::storePacket(const wcpp::Packet& packet) {
  uint16_t kvs_key = (packet.packet_id() & wcpp::packet_id_mask) | ((uint16_t)packet.component_id() << 8);
  return kvs_.write(kvs_key, packet.encode() + 3, packet.size() - 3);
}

wcpp::Packet Kernel::loadPacket(uint8_t packet_id, uint8_t component_id) {
  uint16_t kvs_key = (packet_id & wcpp::packet_id_mask) | ((uint16_t)component_id << 8);
  if (!kvs_.exists(kvs_key)) return wcpp::Packet::null();
  uint8_t size = kvs_.sizeOf(kvs_key) + 3;
  wcpp::Packet packet = allocPacket(size);
  packet.getBuf()[0] = size;
  packet.getBuf()[1] = packet_id;
  packet.getBuf()[2] = component_id;
  kvs_.read(kvs_key, packet.getBuf() + 3);
  return packet;
}

}

