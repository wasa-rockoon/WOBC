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

// Pins
#define LORA_CHANNEL 10
#define LORA_TX_PIN 38
#define LORA_RX_PIN 39
#define LORA_AUX_PIN 40
#define LORA_M0_PIN 12
#define LORA_M1_PIN 11

// Settings
constexpr uint8_t module_id = 0x54;
constexpr uint8_t unit_id = 0x61;

// IO
driver::GenericSerial<HardwareSerial> lora_serial(Serial1);

// Core
core::SerialBus serial_bus(Serial);
interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());


// Components
component::LiPoPower power(Wire, ST, PG, STAT1, STAT2, HEAT, CHARGELED, TEMP, 1);
component::LoRa lora(lora_serial, LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN, LORA_CHANNEL, 0);


kernel::Listener all_packets_;

class Main : public process::Component {
public:
    Main() : process::Component("main", 0x00) {}

    void setup() override {
    }

    void loop() override {
        delay(1000);
        wcpp::Packet packet1 = newPacket(64);
        packet1.telemetry('s', 0x01);
        packet1.append("Sc").setBool(0);
        packet1.append("Vp").setInt(4000);
        packet1.append("Ip").setInt(100);
        packet1.append("Vb").setInt(4000);
        packet1.append("Pp").setInt(100);
        packet1.append("Vd").setInt(3300);
        packet1.append("Id").setInt(100);
        packet1.append("Pd").setInt(100);
        sendPacket(packet1);
    }

} main_;

void setup() {
    // 初期化コード
    Serial.begin(115200);

    kernel::setUnitId(unit_id);
    if (!kernel::begin(module_id, true)) return;

    Serial0.setPins(2, 1);
    Serial1.setPins(LORA_RX_PIN, LORA_TX_PIN);

    Wire.begin(17, 16);
    //serial_bus.begin();

    delay(1000); // 1秒待機

    status_indicator.begin(); // ステータスインジケータの初期化
    status_indicator.blink_on_change(); // ステータスインジケータの点滅

    error_indicator.begin(); // エラーインジケータの初期化
    error_indicator.set(true); // エラーインジケータをONに設定

    //power.begin(); // LiPo電源モジュールの初期化
    lora.begin();  // LoRaの初期化
    main_.begin(); // メインモジュールの初期化

    error_indicator.set(false); // エラーインジケータをOFFに設定
    error_indicator.blink_on_change(100); // エラーインジケータの点滅を設定
}

void loop() {
    status_indicator.update(); // ステータスインジケータの更新
    error_indicator.update();  // エラーインジケータの更新
}
