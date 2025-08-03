#include "telemeter.h"

namespace component {

Telemeter::Telemeter(void)
  : process::Component("Telemeter", component_id) {
}

void Telemeter::webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected to url: %s\n", payload);
      // Send message to server when connected
      break;
    case WStype_TEXT:
      Serial.printf("[WSc] get text: %s\n", payload);
      // Send message to server
      // webSocket.sendTXT("message here");
      break;
    case WStype_BIN:
      break;
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}

void Telemeter::setup() {
    //up_packets_.telemetry();
    up_packets_.packet('A');
    listen(up_packets_, 8);

    delay(5000);

  WiFiMulti_.addAP("Redmi 9T", "1ea4c967d163p");

  WiFi.disconnect();
  while (WiFiMulti_.run() != WL_CONNECTED) {
    delay(100);
  }

  // Server address, port and URL
  webSocket_.begin("54.248.18.111", 80, "http://54.248.18.111/ws");

  // Event handler
  webSocket_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    this->webSocketEvent(type, payload, length);
  });

  // Set reconnect interval
  webSocket_.setReconnectInterval(5000);
}

void Telemeter::loop() {
    if(!webSocket_.isConnected()){
        LOG("mistake connect server");
        webSocket_.begin("54.248.18.111", 80, "http://54.248.18.111/ws");
        delay(1000);
    }

  while (up_packets_) {

    const wcpp::Packet packet = up_packets_.pop();
    uint8_t buf[wcpp::size_max];
    wcpp::Packet packet_tele = wcpp::Packet::empty(buf, wcpp::size_max);

    if (packet.isLocal()) {
      if (packet.isCommand()) {
        packet_tele.command(
          packet.packet_id(), packet.component_id(), kernel::unit_id(), kernel::unit_id());
      } else {
        packet_tele.telemetry(
          packet.packet_id(), packet.component_id(), kernel::unit_id(), kernel::unit_id());
      }
      packet_tele.copyPayload(packet);
    } else {
      packet_tele.copy(packet);
    }

    packet_tele.append("Ts").setInt(millis()); // Add timestamp in ms

    bool ok = webSocket_.sendBIN(packet_tele.encode(), packet_tele.size());

    if(ok) LOG("UP server");
    else LOG("Mistake up");
  }
}
}
