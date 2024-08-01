#pragma once

#include "library/process/task.h"
namespace core {


constexpr unsigned serial_bus_packet_queue_size = 16;

class SerialBus: public process::Task {
public:
  SerialBus(Stream& serial);

  void begin();

protected:
  Stream& serial_;
  Listener all_packets;

  uint8_t rx_buf_[wcpp::size_max + 2];
  unsigned rx_count_;

  void setup() override;
  void loop() override;
};

}

