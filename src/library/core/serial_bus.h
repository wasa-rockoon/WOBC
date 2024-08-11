#pragma once

#include "library/process/task.h"

namespace core {

#ifndef WOBC_SERIAL_BUS_PACKET_QUEUE_SIZE
#define WOBC_SERIAL_BUS_PACKET_QUEUE_SIZE 8
#endif

#ifndef WOBC_SERIAL_BUS_PRIORITY
#define WOBC_SERIAL_BUS_PRIORITY 7
#endif

#ifndef WOBC_SERIAL_BUS_STACK_SIZE
#define WOBC_SERIAL_BUS_STACK_SIZE 4024
#endif

class SerialBus: public process::CoreTask {
public:
  SerialBus(Stream& serial);

protected:
  Stream& serial_;
  Listener all_packets;

  uint8_t rx_buf_[wcpp::size_max + 2];
  unsigned rx_count_;

  void setup() override;
  void loop() override;
};

}

