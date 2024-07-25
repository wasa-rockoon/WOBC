#pragma once

#include "library/process/task.h"
namespace core {

class SerialBus: public process::Task {
public:
  SerialBus(Stream& serial);

protected:
  Stream& serial_;
  Listener all_packets;

  uint8_t rx_buf_[wcpp::size_max + 1];
  // uint8_t tx_buf_[wcpp::size_max + 2];
  unsigned rx_count_;

  void setup() override;
  void loop() override;
};

}

