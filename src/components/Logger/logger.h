#pragma once
#include <library/wobc.h>
#include <SPI.h>

namespace component {

class Logger: public process::Component {
public:
  static const uint8_t component_id = 0x20; // TODO

  Logger(SPIClass& spi);

protected:
  SPIClass& spi_;

  Listener all_packets_;

  void setup() override;
  void loop() override;
};

}