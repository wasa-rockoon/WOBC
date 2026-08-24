// #define NDEBUG

#include <library/wobc.h>
#include <components/LiPoPower/lipo_power.h>
#include <components/LoRa/lora.h>
#include <components/Pressure/pressure.h>
#include <components/GPS/gps.h>
#include <components/Logger/logger.h>
#include <SPI.h>

#define SPI0_SCK_PIN 1
#define SPI0_MOSI_PIN 4
#define SPI0_MISO_PIN 3
#define SPI0_CS_PIN 2

#define SD_INSERTED_PIN 5
#define SDCARD_MOSI_PIN SPI0_MOSI_PIN
#define SDCARD_MISO_PIN SPI0_MISO_PIN
#define SDCARD_SS_PIN SPI0_CS_PIN
#define SDCARD_SCK_PIN SPI0_SCK_PIN

#define ST 6
#define PG 10
#define STAT1 43
#define STAT2 44
#define HEAT 9
#define CHARGELED 8
#define TEMP 7

#define LORA_CHANNEL 3
#define LORA_TX_PIN 38
#define LORA_RX_PIN 39
#define LORA_AUX_PIN 40
#define LORA_M0_PIN 12
#define LORA_M1_PIN 11

constexpr uint8_t module_id = 0x54;
constexpr uint8_t unit_id = 0x61;

HardwareSerial lora_serial(1);
core::SerialBus serial_bus(Serial);

component::LiPoPower power(Wire, ST, PG, STAT1, STAT2, HEAT, CHARGELED, TEMP, unit_id, 1);
component::LoRa lora(LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN, LORA_TX_PIN, LORA_RX_PIN, LORA_CHANNEL, 0);
component::Logger logger(SPI, SPI0_CS_PIN, SD_INSERTED_PIN);
component::Pressure pressure(Wire, unit_id);
component::GPS gps(47, 48, 115200, unit_id);


interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

class Main : public process::Component {
public:
    Main() : process::Component("main", 0x00) {}
    kernel::Listener uplink_listener_;

    void setup() override {
        listen(uplink_listener_, 8);
    }

    void loop() override {
        while (uplink_listener_) {
            wcpp::Packet rx_packet = uplink_listener_.pop();

            // ACK対象はLoRa受信パケットだけに限定する。送信コマンドや
            // 自分で生成したACKを再処理すると、Tracker内で無限ループになる。
            if (!rx_packet.find("Ss")) continue;

            // PC コマンド ('t' または 'c') 以外のパケット (自動送信センサーデータ等) は無視
            char pid = rx_packet.packet_id();
            if (pid != 't' && pid != 'c') continue;

            // 1. ログ出力
            LOG("Uplink Received! Packet ID: '%c' (0x%02X)", rx_packet.packet_id(), rx_packet.packet_id());

            // 2. ACKパケットの作成 (Packet ID 'a', Status "St"=0, 受信Packet ID "Ri")
            wcpp::Packet ack_packet = newPacket(32);
            ack_packet.telemetry('a', rx_packet.component_id());
            ack_packet.append("St").setInt(0);                    // 0: 成功
            ack_packet.append("Ri").setInt(rx_packet.packet_id()); // 受信したパケットID

            // 3. ACKパケットを LoRa 送信用コマンドパケット ("Pa" エントリ) に包んで返信
            wcpp::Packet lora_send_packet = newPacket(ack_packet.size() + 32);
            lora_send_packet.command(lora.send_command_id, lora.component_id_base + 0);
            lora_send_packet.append("Pa").setPacket(ack_packet);
            
            // 4. LoRaコンポーネントへ送出
            sendPacket(lora_send_packet, uplink_listener_);
        }
    }
} main_;

void setup() {
    Serial.begin(115200);
    kernel::setUnitId(unit_id);
    if (!kernel::begin(module_id, true)) return;

    Serial0.setPins(2, 1);
    Wire.begin(17, 16);
    serial_bus.begin();

    SPI.begin(SDCARD_SCK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN, SDCARD_SS_PIN);

    delay(1000); 

    status_indicator.begin();
    status_indicator.blink_on_change();

    error_indicator.begin();
    error_indicator.set(true);

    power.begin();
    lora.begin();
    pressure.begin();
    gps.begin();
    logger.begin();
    main_.begin();

    error_indicator.set(false);
    error_indicator.blink_on_change(100);
}

void loop() {
    status_indicator.update();
    error_indicator.update();
}
