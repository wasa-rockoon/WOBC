#include "aws_forwarder.h"
#include "config_select.h"

#ifndef WASA_HEARTBEAT_INTERVAL_MS
#define WASA_HEARTBEAT_INTERVAL_MS 10000UL
#endif

AwsForwarder::AwsForwarder()
    : process::Component("AWSForwarder", component_id, 1, 6144) {
  priority_ = 1;
}

bool AwsForwarder::isDownlinkUnit(uint8_t unit_id) {
  return unit_id == 0x61 || unit_id == 0x62 || unit_id == 0x41;
}

void AwsForwarder::setup() {
  telemetry_packets_.telemetry();
  listen(telemetry_packets_, 64, true);

  if (!WASA_ENABLE_AWS_OUTPUT) return;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WASA_WIFI_SSID, WASA_WIFI_PASSWORD);
  last_wifi_attempt_ms_ = millis();
}

void AwsForwarder::maintainWifi(uint32_t now) {
  if (!WASA_ENABLE_AWS_OUTPUT || WiFi.status() == WL_CONNECTED) return;

  if (web_socket_configured_) {
    web_socket_.disconnect();
    web_socket_configured_ = false;
    web_socket_connected_ = false;
  }

  if (now - last_wifi_attempt_ms_ < WASA_WIFI_RETRY_INTERVAL_MS) return;
  last_wifi_attempt_ms_ = now;
  WiFi.disconnect(false, false);
  WiFi.begin(WASA_WIFI_SSID, WASA_WIFI_PASSWORD);
}

void AwsForwarder::handleWebSocketEvent(WStype_t type, uint8_t*, size_t) {
  if (type == WStype_CONNECTED) {
    web_socket_connected_ = true;
    last_heartbeat_ms_ = 0;
  } else if (type == WStype_DISCONNECTED || type == WStype_ERROR) {
    web_socket_connected_ = false;
  }
}

void AwsForwarder::configureWebSocket() {
  if (!WASA_ENABLE_AWS_OUTPUT || web_socket_configured_ ||
      WiFi.status() != WL_CONNECTED) return;

  path_ = WASA_WS_PATH;
  path_ += path_.indexOf('?') >= 0 ? '&' : '?';
  path_ += "client_name=";
  path_ += WASA_RECEIVER_ID;

  if (WASA_ENABLE_AUTH && strlen(WASA_TELEMETRY_INGEST_TOKEN) > 0U) {
    authorization_header_ = "Authorization: Bearer ";
    authorization_header_ += WASA_TELEMETRY_INGEST_TOKEN;
    web_socket_.setExtraHeaders(authorization_header_.c_str());
  } else {
    web_socket_.setExtraHeaders();
  }

  web_socket_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    handleWebSocketEvent(type, payload, length);
  });
  web_socket_.setReconnectInterval(WASA_WEBSOCKET_RECONNECT_INTERVAL_MS);

  if (WASA_WS_USE_TLS) {
    web_socket_.beginSSL(WASA_WS_HOST, WASA_WS_PORT, path_.c_str());
  } else {
    web_socket_.begin(WASA_WS_HOST, WASA_WS_PORT, path_.c_str());
  }
  web_socket_configured_ = true;
}

void AwsForwarder::sendHeartbeat(uint32_t now) {
  if (!web_socket_connected_) return;
  if (last_heartbeat_ms_ != 0 &&
      now - last_heartbeat_ms_ < WASA_HEARTBEAT_INTERVAL_MS) return;

  last_heartbeat_ms_ = now;
  String message = "{\"kind\":\"receiver_heartbeat_v1\",\"receiver_id\":\"";
  message += WASA_RECEIVER_ID;
  message += "\",\"uptime_ms\":";
  message += String(now);
  message += "}";
  web_socket_.sendTXT(message);
}

void AwsForwarder::forwardAvailablePackets() {
  for (unsigned handled = 0; handled < 64 && telemetry_packets_; ++handled) {
    const wcpp::Packet packet = telemetry_packets_.pop();
    if (!packet.isRemote() || !isDownlinkUnit(packet.origin_unit_id())) continue;
    if (!WASA_ENABLE_AWS_OUTPUT || !web_socket_connected_) continue;
    web_socket_.sendBIN(packet.encode(), packet.size());
  }
}

void AwsForwarder::loop() {
  const uint32_t now = millis();
  maintainWifi(now);
  configureWebSocket();
  if (web_socket_configured_) web_socket_.loop();
  sendHeartbeat(now);
  forwardAvailablePackets();
  delay(2);
}
