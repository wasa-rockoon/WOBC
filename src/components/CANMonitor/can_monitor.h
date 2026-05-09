#pragma once

#include <library/wobc.h>

namespace component {

class CANMonitor : public process::Component {
public:
  static const uint8_t component_id = 0x51;

  CANMonitor();

protected:
  Listener can_packets_;

  void setup() override;
  void loop() override;
};

}