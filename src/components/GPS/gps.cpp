#include "gps.h"
#include <Arduino.h>

namespace component {
GPS::GPS(driver::GenericSerialClass& serial, uint32_t baud)
  : process::Component("GPS", component_id),
  serial_(serial), baud_(baud) {    
}

void GPS::setup(){
  serial_.begin(baud_);
}

void GPS::loop() {
  int start = millis();
  while(serial_.available() && millis()-start<500) {
    if (gps_.encode(serial_.read())) {
      wcpp::Packet packet = newPacket(64);
      packet.telemetry(telemetry_id, component_id, kernel::unit_id(), 0xFF, 1234);
      packet.append("LA").setFloat64(gps_.location.lat());
      packet.append("LO").setFloat64(gps_.location.lng());
      packet.append("AL").setInt((int)gps_.altitude.meters());
      //packet.append("UT").setint();
      sendPacket(packet);
    }
  }
}

}
