#pragma once
#include <WiFi.h>
#include <components/Telemeter/src/WebSocketsClient.h>
#include <library/wobc.h>

class AwsForwarder : public process::Component {
public:
  static constexpr uint8_t component_id = 0x50;
  AwsForwarder();
protected:
  void setup() override;
  void loop() override;
private:
  WebSocketsClient web_socket_;
  Listener telemetry_packets_;
  bool web_socket_configured_ = false;
  bool web_socket_connected_ = false;
  uint32_t last_wifi_attempt_ms_ = 0;
  uint32_t last_heartbeat_ms_ = 0;
  String path_;
  String authorization_header_;
  void maintainWifi(uint32_t now);
  void configureWebSocket();
  void handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
  void sendHeartbeat(uint32_t now);
  void forwardAvailablePackets();
  static bool isDownlinkUnit(uint8_t unit_id);
};
