// Tracker / MissionBusのLoRaテレメトリを2つのUARTで受信する。
#include <Arduino.h>
#include <library/wobc.h>
#include <components/LoRa/e220.h>

// LoRa2026基板のLoRa2 (U401)。ピン番号はESP32-S3側のGPIO。
// LoRa1 (U301)は下のmission_loraでMissionBus用に設定する。
#define LORA_CHANNEL 4 // modules/Tracker/main.cppと一致させる。
#define LORA_TX_PIN 7
#define LORA_RX_PIN 18
#define LORA_AUX_PIN 8
#define LORA_M0_PIN 5
#define LORA_M1_PIN 6
#define LORA_SW_A1 9
#define LORA_SW_A2 10

// 元のmodules/LoRa/main.cppと同様、通常はUSBシリアルで接続する。
#ifndef LORA_USE_CAN
#define LORA_USE_CAN 0
#endif

namespace {

constexpr uint8_t module_id = 0x4C;
constexpr unsigned long lora_baud = 115200;
constexpr unsigned rssi_entry_size = 4; // Ss + 符号付きRSSI(-256..-1)

// PCへの出力は[WCPP][CRC8][0x00]。Serial.print()による文字列を混在させない。
core::SerialBus serial_bus(Serial);
#if LORA_USE_CAN
core::CANBus can_bus(44, 43); // RX, TX
#endif
interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

// 受信専用。PCからの's'コマンドによる無線送信は行わない。
class LoRaReceiver : public process::Component {
public:
  LoRaReceiver(const char* name, uint8_t id, uint8_t uart, uint8_t channel,
               pin_t tx, pin_t rx, pin_t aux, pin_t m0, pin_t m1,
               pin_t sw_a1, pin_t sw_a2)
    : process::Component(name, id),
      lora_serial_(uart), e220_(lora_serial_, aux, m0, m1),
      channel_(channel), tx_(tx), rx_(rx), aux_(aux), m0_(m0), m1_(m1),
      sw_a1_(sw_a1), sw_a2_(sw_a2) {
    priority_ = 1;
  }

  bool initialize() {
    failed_step_ = nullptr;
    pinMode(sw_a1_, OUTPUT);
    pinMode(sw_a2_, OUTPUT);
    digitalWrite(sw_a1_, HIGH);
    digitalWrite(sw_a2_, LOW);

    // 最大フレーム(長さ + データ255バイト + RSSI)を保持できる容量。
    if (!checkInitStep("uart-buffer", lora_serial_.setRxBufferSize(512) >= 512)) return false;
    lora_serial_.begin(9600, SERIAL_8N1, rx_, tx_);
    if (!checkInitStep("uart-begin", static_cast<bool>(lora_serial_))) return false;

    if (!checkInitStep("begin-aux", e220_.begin())) return false;
    ::delay(1000);
    if (!checkInitStep("config-mode", e220_.setMode(E220::Mode::CONFIG_DS))) return false;

    // Trackerのcomponents/LoRa/lora.cppと同じ無線設定。
    const bool configured =
      checkInitStep("defaults", e220_.setParametersToDefault()) &&
      checkInitStep("baud", e220_.setSerialBaudRate(lora_baud)) &&
      checkInitStep("air-rate", e220_.setDataRate(E220::SF::SF9, E220::BW::BW125kHz)) &&
      checkInitStep("env-rssi", e220_.setEnvRSSIEnable(true)) &&
      checkInitStep("send-mode", e220_.setSendMode(E220::SendMode::TRANSPARENT)) &&
      checkInitStep("address", e220_.setModuleAddr(E220::BROADCAST)) &&
      checkInitStep("channel", e220_.setChannel(channel_)) &&
      checkInitStep("rssi", e220_.setRSSIEnable(true));

    // 設定失敗時も通常モードへ戻す。設定中のUARTは9600bps。
    const bool normal_mode = checkInitStep("normal-mode", e220_.setMode(E220::Mode::NORMAL));
    lora_serial_.flush();
    lora_serial_.updateBaudRate(lora_baud);
    ::delay(100);
    return configured && normal_mode;
  }

  bool start() {
    if (!initialize()) {
      // WCPPのエラーパケットに収まる短い診断。最初の失敗時点の値を使う。
      error("lrIN", "%s A=%d M=%d%d R=%d", failed_step_,
            failed_aux_, failed_m0_, failed_m1_, failed_rx_pending_);
      return false;
    }
    if (!begin()) {
      error("lrST", "LoRa task start failed");
      return false;
    }
    LOG("LoRa receiver ready (channel %u).", channel_);
    return true;
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
  bool checkInitStep(const char* step, bool ok) {
    if (ok) return true;
    // 後処理でモードや受信バッファが変わる前に、最初の失敗を記録する。
    if (failed_step_ == nullptr) {
      failed_step_ = step;
      failed_aux_ = digitalRead(aux_);
      failed_m0_ = digitalRead(m0_);
      failed_m1_ = digitalRead(m1_);
      failed_rx_pending_ = lora_serial_.available();
    }
    return false;
  }

  // 宣言順もUART -> E220にし、有効なStreamを渡す。
  HardwareSerial lora_serial_;
  E220 e220_;
  const uint8_t channel_;
  const pin_t tx_, rx_, aux_, m0_, m1_, sw_a1_, sw_a2_;
  const char* failed_step_ = nullptr;
  int failed_aux_ = 0, failed_m0_ = 0, failed_m1_ = 0, failed_rx_pending_ = 0;
};

// LoRa2 / U401: Tracker (unit 0x61), UART1, channel 3.
LoRaReceiver tracker_lora("TrackerLoRa", 0x10, 1, LORA_CHANNEL,
    LORA_TX_PIN, LORA_RX_PIN, LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN,
    LORA_SW_A1, LORA_SW_A2);
// LoRa1 / U301: MissionBus (unit 0x62), UART2, channel 11.
LoRaReceiver mission_lora("MissionLoRa", 0x11, 2, 11,
    13, 12, 11, 14, 21, 39, 40);

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

  // 片側が故障していても、もう片側の受信タスクを起動する。
  const bool tracker_ok = tracker_lora.start();
  const bool mission_ok = mission_lora.start();
  if (!tracker_ok || !mission_ok) return;
  error_indicator.set(false);
  error_indicator.blink_on_change(100);
}

void loop() {
  status_indicator.update();
  error_indicator.update();
  delay(1);
}
