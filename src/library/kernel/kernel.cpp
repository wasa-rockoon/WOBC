#include "kernel.h"

namespace kernel {

Kernel kernel_;

Kernel::Kernel()
: packet_heap_(packet_heap_arena_, WOBC_PACKET_HEAP_SIZE), 
  mutex_(xSemaphoreCreateMutex()) {
}

bool Kernel::begin(uint8_t module_id, bool overwrite_module_id) {
  kvs_.begin();

  // check module id
  if (kvs_.exists(module_id_kvs_key) && kvs_.sizeOf(module_id_kvs_key == 1)) {
    uint8_t module_id_stored;
    kvs_.read(module_id_kvs_key, &module_id_stored);

    if (overwrite_module_id) {
      if (module_id != module_id_stored) kvs_.write(module_id_kvs_key, &module_id, 1);  
    }
    else {
      assert(module_id == module_id_stored);
      return false;
    }
  }
  else {
    kvs_.write(module_id_kvs_key, &module_id, 1);  
  }

  kernel_.module_id_ = module_id;
  timer_handle_ = xTimerCreate(
    "Kernel", 1000, pdTRUE, this, timerEntryPoint);
  return true;
}

void Kernel::timerEntryPoint(TimerHandle_t timer_handle) {
  wcpp::Packet heartbeat = kernel_.allocPacket(32);
  heartbeat.telemetry(packet_id_heartbeat, module_id());
  heartbeat.append("Im").setInt(module_id());
  if (unit_id() != unit_id_local) {
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
      if (unit_id_ == unit_id_local) {
        unit_id_ = (*e).getInt(); 
      }
      else {
        assert((*e).getInt() == unit_id_local);
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
    packet_copied = packet;
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

}

