#pragma once
#include <library/wobc.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include "src/WebSocketsClient.h"

namespace component {

class Telemeter : public process::Component {
public:
  static const uint8_t component_id = 0x50;
  static const uint8_t telemeter_id = 'W';

  Telemeter(void);

protected:
  WiFiMulti WiFiMulti_;
  WebSocketsClient webSocket_;
  
  Listener up_packets_;
  unsigned packets_wrote_;
  unsigned bytes_wrote_;

  void setup() override;
  void loop() override;
  void webSocketEvent(WStype_t type, uint8_t* payload, size_t length);
};

}
