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

  // Keep GS's packet-A filter by default; opt in to all telemetry for a gateway.
  explicit Telemeter(bool all_telemetry = false);

protected:
  WiFiMulti WiFiMulti_;
  WebSocketsClient webSocket_;
  
  Listener up_packets_;
  bool all_telemetry_;
  unsigned packets_wrote_;
  unsigned bytes_wrote_;

  void setup() override;
  void loop() override;
  void webSocketEvent(WStype_t type, uint8_t* payload, size_t length);
};

}
