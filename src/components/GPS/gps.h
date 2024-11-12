#include <library/wobc.h>
#include "TinyGPSPlus.h"

namespace component{

class GPS : public process::Component{
public:
    static const uint8_t component_id = 21;
    static const uint8_t telemetry_id = 'M';

public:
  GPS(driver::GenericSerialClass& serial, uint32_t baud);

  void setup() override;
  void loop() override;

protected:
  driver::GenericSerialClass& serial_;
  uint32_t baud_;
  TinyGPSPlus gps_;
};

}