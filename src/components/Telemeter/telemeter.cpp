#include "telemeter.h"

namespace component {

Telemeter::Telemeter(void)
  : process::Component("Telemeter", component_id) {
}

bool Telemeter::connectToWiFi() {
  LOG("WiFiに接続中...");
  unsigned long startTime = millis();
  while (WiFiMulti_.run() != WL_CONNECTED) {
    delay(100);
    // 10秒以上経過したら接続を諦める
    if (millis() - startTime > 10000) {
      LOG("WiFi接続タイムアウト");
      return false;
    }
  }
  LOG("WiFi接続成功");
  return true;
}

// テキストデータをWebSocketで送信するデバッグ用関数
bool Telemeter::sendDebugText(const String& message) {
  if (WiFi.status() != WL_CONNECTED) {
    LOG("デバッグメッセージ送信失敗: WiFi未接続");
    return false;
  }
  
  String debugMsg = "[DEBUG] " + message;
  bool result = webSocket_.sendTXT(debugMsg);
  
  if (result) {
    String logMsg = "デバッグメッセージ送信: " + message;
    LOG(logMsg.c_str());  // String型をconst char*に変換
  } else {
    LOG("デバッグメッセージ送信失敗");
  }
  
  return result;
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
  up_packets_.unit_origin('a');
  listen(up_packets_, 8);

  delay(5000);

  WiFiMulti_.addAP("としや", "f2gthy456");

  WiFi.disconnect();
  connectToWiFi();

  // Server address, port and URL
  webSocket_.begin("18.178.150.196", 80, "/ws");

  // Event handler
  webSocket_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    this->webSocketEvent(type, payload, length);
  });

  // Set reconnect interval
  webSocket_.setReconnectInterval(5000);
}

void Telemeter::loop() {
  // WiFi接続状態チェックと表示
  static unsigned long lastCheckTime = 0;
  
  // 5秒ごとに接続状態をチェック
  if (millis() - lastCheckTime > 5000) {
    lastCheckTime = millis();
    
    if (WiFi.status() == WL_CONNECTED) {
      LOG("WiFi接続状態: 正常");
      sendDebugText("WiFi接続状態: 正常");
    } else {
      LOG("WiFi切断を検出。再接続中...");
      connectToWiFi();
    }
  }

  webSocket_.loop();
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
