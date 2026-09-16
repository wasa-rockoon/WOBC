// ESP32-S3 LoRa2026基板のLoRa2でチャンネル16を受信し、PCへWCPPで転送する。
#include <Arduino.h>
#include <library/wobc.h>
#include <components/LoRa/e220.h>

// LoRa2 / U401。GPIO番号はESP32-S3側（TXはE220のRXDへ接続）。
#define LORA_CHANNEL 16
#define LORA_TX_PIN 7
#define LORA_RX_PIN 18
#define LORA_AUX_PIN 8
#define LORA_M0_PIN 5
#define LORA_M1_PIN 6
#define LORA_SW_A1 9
#define LORA_SW_A2 10

#ifndef LORA_USE_CAN
#define LORA_USE_CAN 0
#endif

namespace {

constexpr uint8_t module_id = 0x4C;
constexpr unsigned long lora_baud = 115200;
constexpr unsigned rssi_entry_size = 4; // Ss + 符号付きRSSI(-256..-1)

// バイナリWCPPを使うため、Serial.print()の文字列は混在させない。
core::SerialBus serial_bus(Serial);
#if LORA_USE_CAN
core::CANBus can_bus(44, 43); // RX, TX
#endif
interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

class LoRaReceiver : public process::Component {
public:
  LoRaReceiver()
    : process::Component("LoRa16", 0x10), lora_serial_(1),
      e220_(lora_serial_, LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN) {
    priority_ = 1;
  }

  bool start() {
    if (!initialize()) {
      error("lrIN", "%s A=%d M=%d%d R=%d", failed_step_,
            failed_aux_, failed_m0_, failed_m1_, failed_rx_pending_);
      return false;
    }
    if (!begin()) {
      error("lrST", "LoRa task start failed");
      return false;
    }
    LOG("LoRa2 ready (channel %u).", LORA_CHANNEL);
    return true;
  }

protected:
  void loop() override {
    uint8_t data[255];
    const unsigned len = e220_.receive(data, sizeof(data));
    if (len == 0) return;

    // 無線は[長さ][WCPP][CRC8]。receive()は外側の長さとRSSIを除去する。
    // decode()に渡す前に、WCPPの長さ・ヘッダー・チェックサムを確認する。
    if (len < 5 || data[0] != len - 1) {
      error("lrSZ", "LoRa frame length mismatch");
      return;
    }
    const unsigned packet_size = len - 1;
    const unsigned header_size = data[3] == wcpp::unit_id_local ? 4 : 7;
    if (packet_size < header_size || packet_size + rssi_entry_size > wcpp::size_max) {
      error("lrSZ", "LoRa packet length invalid");
      return;
    }
    if (wcpp::Packet::checksum(data, packet_size) != data[packet_size]) {
      error("lrCS", "LoRa checksum mismatch");
      return;
    }

    const wcpp::Packet received = wcpp::Packet::decode(data);
    wcpp::Packet packet = newPacket(packet_size + rssi_entry_size);
    if (!packet) return;
    if (!packet.copy(received) || !packet.append("Ss").setInt(e220_.getRSSI())) {
      error("lrSS", "LoRa RSSI append failed");
      return;
    }
    // 送信元・宛先・ID・ペイロードを保持。SerialBusがCRC8と終端を付ける。
    sendPacket(packet);
  }

private:
  bool initialize() {
    pinMode(LORA_SW_A1, OUTPUT);
    pinMode(LORA_SW_A2, OUTPUT);
    digitalWrite(LORA_SW_A1, HIGH);
    digitalWrite(LORA_SW_A2, LOW);

    // 最大フレーム（長さ + データ255バイト + RSSI）を収容する。
    if (!checkInitStep("uart-buffer", lora_serial_.setRxBufferSize(512) >= 512)) return false;
    lora_serial_.begin(9600, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
    if (!checkInitStep("uart-begin", static_cast<bool>(lora_serial_))) return false;
    if (!checkInitStep("begin-aux", e220_.begin())) return false;
    ::delay(1000);
    if (!checkInitStep("config-mode", e220_.setMode(E220::Mode::CONFIG_DS))) return false;

    // 送信側もチャンネル16・SF9・BW125kHz・透過モードに合わせる。
    const bool configured =
      checkInitStep("defaults", e220_.setParametersToDefault()) &&
      checkInitStep("baud", e220_.setSerialBaudRate(lora_baud)) &&
      checkInitStep("air-rate", e220_.setDataRate(E220::SF::SF9, E220::BW::BW125kHz)) &&
      checkInitStep("env-rssi", e220_.setEnvRSSIEnable(true)) &&
      checkInitStep("send-mode", e220_.setSendMode(E220::SendMode::TRANSPARENT)) &&
      checkInitStep("address", e220_.setModuleAddr(E220::BROADCAST)) &&
      checkInitStep("channel", e220_.setChannel(LORA_CHANNEL)) &&
      checkInitStep("rssi", e220_.setRSSIEnable(true));

    uint8_t channel = 0;
    const bool channel_verified = configured &&
      checkInitStep("channel-read", e220_.readRegister(E220::ADDR::REG2, &channel)) &&
      checkInitStep("channel-check", channel == LORA_CHANNEL);

    // 設定失敗時も通常モードへ戻す。設定モードのUARTは9600bps。
    const bool normal_mode = checkInitStep("normal-mode", e220_.setMode(E220::Mode::NORMAL));
    lora_serial_.flush();
    lora_serial_.updateBaudRate(lora_baud); // RX/TXのピン割り当てを維持する。
    ::delay(100);
    return channel_verified && normal_mode;
  }

  bool checkInitStep(const char* step, bool ok) {
    if (ok) return true;
    // 後処理で状態が変わる前に、最初の失敗を記録する。
    if (failed_step_ == nullptr) {
      failed_step_ = step;
      failed_aux_ = digitalRead(LORA_AUX_PIN);
      failed_m0_ = digitalRead(LORA_M0_PIN);
      failed_m1_ = digitalRead(LORA_M1_PIN);
      failed_rx_pending_ = lora_serial_.available();
    }
    return false;
  }

  // E220が参照するUARTを先に構築する。
  HardwareSerial lora_serial_;
  E220 e220_;
  const char* failed_step_ = nullptr;
  int failed_aux_ = 0, failed_m0_ = 0, failed_m1_ = 0, failed_rx_pending_ = 0;
};

LoRaReceiver lora;

} // namespace

void setup() {
  Serial.begin(115200);
  status_indicator.begin();
  error_indicator.begin();
  error_indicator.set(true);

  if (!kernel::begin(module_id, false)) return;
  status_indicator.blink_on_change();
  serial_bus.begin();
#if LORA_USE_CAN
  can_bus.begin();
#endif

  if (!lora.start()) return;
  error_indicator.set(false);
  error_indicator.blink_on_change(100);
}

void loop() {
  status_indicator.update();
  error_indicator.update();
  delay(1);
}

