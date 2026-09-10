// TrackerのLoRaテレメトリを受信し、WCPP形式でPCへ転送する。
#include <Arduino.h>
#include <library/wobc.h>
#include <components/LoRa/e220.h>
#include "hardware.h"

// LoRa2026基板のLoRa2 (U401)。ピン番号はESP32-S3側のGPIO。
// LoRa1 (U301)を使う場合: TX=13, RX=12, AUX=11, M0=14, M1=21,
// SW_A1=39, SW_A2=40。チャンネルは通信相手と合わせる。
#define LORA_CHANNEL 3 // modules/Tracker/main.cppと一致させる。
// 元のmodules/LoRa/main.cppと同様、通常はUSBシリアルで接続する。
#ifndef LORA_USE_CAN
#define LORA_USE_CAN 0
#endif

namespace {

constexpr uint8_t module_id = 0x4C;
constexpr uint8_t lora_component_id = 0x10;
constexpr unsigned long lora_baud = 115200;
constexpr unsigned rssi_entry_size = 4; // Ss + 符号付きRSSI(-256..-1)

// PCへの出力は[WCPP][CRC8][0x00]。Serial.print()による文字列を混在させない。
core::SerialBus serial_bus(Serial);
#if LORA_USE_CAN
core::CANBus can_bus(lora_esp_hardware::can::rx,
                     lora_esp_hardware::can::tx);
#endif
interface::WatchIndicator<unsigned> status_indicator(
    lora_esp_hardware::indicator::status, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(
    lora_esp_hardware::indicator::error, kernel::errorCount());

// 受信専用。PCからの's'コマンドによる無線送信は行わない。
class LoRaReceiver : public process::Component {
public:
  LoRaReceiver()
    : process::Component("LoRa", lora_component_id),
      lora_serial_(1),
      e220_(lora_serial_, lora_esp_hardware::lora2::aux,
            lora_esp_hardware::lora2::m0, lora_esp_hardware::lora2::m1) {
    priority_ = 1;
  }

  bool initialize() {
    pinMode(lora_esp_hardware::lora2::rf_switch_1, OUTPUT);
    pinMode(lora_esp_hardware::lora2::rf_switch_2, OUTPUT);
    digitalWrite(lora_esp_hardware::lora2::rf_switch_1, HIGH);
    digitalWrite(lora_esp_hardware::lora2::rf_switch_2, LOW);

    // 最大フレーム(長さ + データ255バイト + RSSI)を保持できる容量。
    if (lora_serial_.setRxBufferSize(512) < 512) return false;
    lora_serial_.begin(9600, SERIAL_8N1,
                       lora_esp_hardware::lora2::uart_rx,
                       lora_esp_hardware::lora2::uart_tx);

    if (!e220_.begin()) return false;
    ::delay(1000);
    if (!e220_.setMode(E220::Mode::CONFIG_DS)) return false;

    // Trackerのcomponents/LoRa/lora.cppと同じ無線設定。
    const bool configured =
      e220_.setParametersToDefault() &&
      e220_.setSerialBaudRate(lora_baud) &&
      e220_.setDataRate(E220::SF::SF9, E220::BW::BW125kHz) &&
      e220_.setEnvRSSIEnable(true) &&
      e220_.setSendMode(E220::SendMode::TRANSPARENT) &&
      e220_.setModuleAddr(E220::BROADCAST) &&
      e220_.setChannel(LORA_CHANNEL) &&
      e220_.setRSSIEnable(true);

    // 設定失敗時も通常モードへ戻す。設定中のUARTは9600bps。
    const bool normal_mode = e220_.setMode(E220::Mode::NORMAL);
    lora_serial_.flush();
    lora_serial_.updateBaudRate(lora_baud);
    ::delay(100);
    return configured && normal_mode;
  }

protected:
  void loop() override {
    uint8_t data[255];
    const unsigned len = e220_.receive(data, sizeof(data));
    if (len == 0) return;

    // Trackerのフレームは[長さ][WCPP][CRC8]。
    // receive()が外側の長さとE220付加のRSSIを除去し、WCPP + CRC8を返す。
    // Packet::decode()は内部の長さを信用するため、コピー前に検証する。
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
    // GPS/気圧の送信元0x61・宛先0xFF・ID・ペイロードはそのまま保持する。
    // Trackerのローカル診断パケットも受信できるよう、送信元で絞り込まない。
    // SerialBusがSs追加後の長さ・CRC8・終端を使ってPCへ出力する。
    sendPacket(packet);
  }

private:
  // 宣言順もUART -> E220にし、有効なStreamを渡す。
  HardwareSerial lora_serial_;
  E220 e220_;
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

  if (!lora.initialize()) {
    lora.error("lrIN", "LoRa setup failed; check wiring and power");
    return;
  }
  if (!lora.begin()) {
    lora.error("lrST", "LoRa task start failed");
    return;
  }
  lora.LOG("Tracker LoRa receiver ready (channel %u).", LORA_CHANNEL);
  error_indicator.set(false);
  error_indicator.blink_on_change(100);
}

void loop() {
  status_indicator.update();
  error_indicator.update();
  delay(1);
}
