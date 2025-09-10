#include "telemeter.h"

namespace component {

Telemeter::Telemeter(void)
  : process::Component("Telemeter", component_id) {}

static void hexdump(const uint8_t* p, size_t n) {
  // 必要ならバイナリの先頭数十バイトを可視化
  size_t show = n < 32 ? n : 32;
  for (size_t i = 0; i < show; ++i) {
    char buf[4];
    sprintf(buf, "%02X ", p[i]);
    Serial.print(buf);
  }
  if (n > show) Serial.print("...");
  Serial.println();
}

void Telemeter::webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] DISCONNECTED (len=%u)\n", (unsigned)length);
      break;

    case WStype_CONNECTED:
      // payload にはサーバURLが来る（ライブラリ実装による）
      Serial.printf("[WSc] CONNECTED url: %s (len=%u)\n", (const char*)payload, (unsigned)length);
      break;

    case WStype_TEXT:
      Serial.printf("[WSc] TEXT: %s\n", (const char*)payload);
      break;

    case WStype_BIN:
      Serial.printf("[WSc] BIN len=%u\n", (unsigned)length);
      break;

    case WStype_ERROR:
      // Links2004/arduinoWebSockets の実装では payload にエラーメッセージ/コードが入ることがある
      Serial.printf("[WSc] ERROR len=%u\n", (unsigned)length);
      if (payload && length) {
        Serial.print("[WSc] ERROR payload: ");
        hexdump(payload, length);
      }
      break;

    case WStype_FRAGMENT_TEXT_START:
      Serial.println("[WSc] FRAGMENT_TEXT_START");
      break;
    case WStype_FRAGMENT_BIN_START:
      Serial.println("[WSc] FRAGMENT_BIN_START");
      break;
    case WStype_FRAGMENT:
      Serial.println("[WSc] FRAGMENT");
      break;
    case WStype_FRAGMENT_FIN:
      Serial.println("[WSc] FRAGMENT_FIN");
      break;
  }
}

void Telemeter::setup() {
  // up_packets_.telemetry();
  up_packets_.unit_origin('a');
  listen(up_packets_, 8);

  delay(5000);

  // ★ Wi-Fi 接続
  WiFiMulti_.addAP("としや", "f2gthy456");
  WiFi.disconnect();
  while (WiFiMulti_.run() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] connected");
  Serial.print("[WiFi] IP: "); Serial.println(WiFi.localIP());

  // ★ WebSocket begin: 第3引数は "パス" のみ（Nginx 現状なら "/" が正解）
  webSocket_.begin("13.230.241.30", 80, "/");

  // イベントハンドラ
  webSocket_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    this->webSocketEvent(type, payload, length);
  });

  // ハートビート（切断検知を可視化）
  webSocket_.enableHeartbeat(15000, 3000, 2); // 15s ping, 3s timeout, 2 retries

  // 再接続間隔
  webSocket_.setReconnectInterval(5000);

  // デバッグヘッダ（任意）
  // webSocket_.setExtraHeaders("User-Agent: esp32-telemeter\r\n");
}

void Telemeter::loop() {
  webSocket_.loop();

  while (up_packets_) {
    const wcpp::Packet packet = up_packets_.pop();

    // 組み立て直し（telemetry/command 判定含む）
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

    packet_tele.append("Ts").setInt(millis()); // タイムスタンプ付与

    // === 送信前の健全性チェック ===
    if (!webSocket_.isConnected()) {
      LOG("WS not connected -> skip send");
      continue;
    }
    const uint8_t* p = packet_tele.encode();
    size_t sz = packet_tele.size();
    if (!p || sz == 0) {
      Serial.printf("[UP] encode error: ptr=%p size=%u\n", p, (unsigned)sz);
      continue;
    }

    // （必要なら）サイズ上限を自衛的にチェック
    // WebSocket フレームの実装上、巨大パケットはフラグメント化が必要になることがあります。
    // ここでは簡単に 8KB を上限にしてみる例：
    const size_t MAX_BIN = 8 * 1024;
    if (sz > MAX_BIN) {
      Serial.printf("[UP] too large payload: %u bytes (limit=%u)\n", (unsigned)sz, (unsigned)MAX_BIN);
      // ここで分割送信 or 破棄などの方針を取る
      continue;
    }

    // 送信
    bool ok = webSocket_.sendBIN(p, sz);

    if (ok) {
      LOG("UP server");
    } else {
      // 失敗理由の可視化
      Serial.printf("[UP] sendBIN failed: connected=%d size=%u\n", webSocket_.isConnected(), (unsigned)sz);
      Serial.print("[UP] first bytes: "); hexdump(p, sz);
      // Mistake up の代わりに具体的な原因を出す
      LOG("SEND FAIL (see above details)");
    }
  }
}

} // namespace component