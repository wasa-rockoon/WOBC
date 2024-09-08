// #define NDEBUG

#include <library/wobc.h>
#include <components/LiPoPower/lipo_power.h>
#include <components/LoRa/lora.h>

#define ST 6
#define PG 10
#define STAT1 43
#define STAT2 44
#define HEAT 9
#define CHARGELED 8
#define TEMP 7

#define LORA_CHANNEL 10
#define LORA_TX_PIN 38
#define LORA_RX_PIN 39
#define LORA_AUX_PIN 40
#define LORA_M0_PIN 12
#define LORA_M1_PIN 11

HardwareSerial lora_serial(1);
core::SerialBus serial_bus(Serial);

component::LiPoPower power(Wire, ST, PG, STAT1, STAT2, HEAT, CHARGELED, TEMP, 1);
component::LoRa lora(LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN, LORA_TX_PIN, LORA_RX_PIN, LORA_CHANNEL, 0);

constexpr uint8_t module_id = 0x54;
constexpr uint8_t unit_id = 0x61;

kernel::Listener all_packets_;

interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

class Main : public process::Component {
public:
    Main() : process::Component("main", 0x00) {}

    void setup() override {
        // パケットのリスナーを設定
        listen(all_packets_, 8);
    }

    void loop() override {
    }

} main_;

void setup() {
    // 初期化コード
    Serial.begin(115200);

    kernel::setUnitId(unit_id); // ユニットIDを設定
    if (!kernel::begin(module_id, true)) return; // モジュールIDをチェック

    Serial0.setPins(2, 1);

    Wire.begin(17, 16); // I2C通信のピン設定
    serial_bus.begin();  // シリアルバスの初期化

    delay(1000); // 1秒待機

    status_indicator.begin(); // ステータスインジケータの初期化
    status_indicator.blink_on_change(); // ステータスインジケータの点滅

    error_indicator.begin(); // エラーインジケータの初期化
    error_indicator.set(true); // エラーインジケータをONに設定

    power.begin(); // LiPo電源モジュールの初期化
    lora.begin();  // LoRaの初期化
    main_.begin(); // メインモジュールの初期化

    error_indicator.set(false); // エラーインジケータをOFFに設定
    error_indicator.blink_on_change(100); // エラーインジケータの点滅を設定
}

void loop() {
    status_indicator.update(); // ステータスインジケータの更新
    error_indicator.update();  // エラーインジケータの更新
}
