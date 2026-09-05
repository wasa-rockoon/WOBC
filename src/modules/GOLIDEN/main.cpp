#include <library/wobc.h>
#include <components/LiPoPower/lipo_power.h>
#include <components/LoRa/lora.h>
#include <components/Pressure/pressure.h>
#include <components/IMU/IMU.h>
#include <components/GPS/gps.h>
#include <components/Logger/logger.h>
#include <SPI.h>

#define SPI0_CS_PIN     13
#define SPI0_MOSI_PIN   14
#define SPI0_MISO_PIN   47
#define SPI0_SCK_PIN    21

#define GPS_RX_PIN 11
#define GPS_TX_PIN 12

#define LORA_CHANNEL 3
#define LORA_TX_PIN 40
#define LORA_RX_PIN 39
#define LORA_AUX_PIN 38
#define LORA_M0_PIN 43
#define LORA_M1_PIN 44

#define ST 6
#define PG 7
#define STAT1 8
#define STAT2 18
#define HEAT -1
#define CHARGELED 10
#define TEMP -1

#define SD_INSERTED_PIN 48
#define SDCARD_MOSI_PIN SPI0_MOSI_PIN
#define SDCARD_MISO_PIN SPI0_MISO_PIN
#define SDCARD_SS_PIN SPI0_CS_PIN
#define SDCARD_SCK_PIN SPI0_SCK_PIN

constexpr uint8_t module_id = 0x4E;  // GOLIDEN module ID
constexpr uint8_t unit_id = 0x63;

core::SerialBus serial_bus(Serial);


component::Logger logger(SPI, SPI0_CS_PIN, SD_INSERTED_PIN);
component::GPS gps(GPS_RX_PIN, GPS_TX_PIN, 9600, unit_id, 1);
component::IMU9 imu(Wire, unit_id, 100, 15.0f);  // 発射検知閾値 35 m/s^2 (~3.6G)
component::LoRa lora(LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN, LORA_TX_PIN, LORA_RX_PIN, LORA_CHANNEL, 0);
component::LiPoPower power(Wire, ST, PG, STAT1, STAT2, HEAT, CHARGELED, TEMP, unit_id, 1);


interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

class Main : public process::Component {
public:
    Main() : process::Component("main", 0x00) {}
    kernel::Listener tx_listener_;
    wcpp::Packet pending_tx_ = wcpp::Packet::null();

    void setup() override {
        listen(tx_listener_, 32);
    }

    void loop() override {
        if (pending_tx_) {
            if (!lora.queuePacket(pending_tx_, component::LoRa::TxPriority::Normal)) return;
            pending_tx_ = wcpp::Packet::null();
        }
        while (tx_listener_) {
            wcpp::Packet packet = tx_listener_.pop();

            if (packet.component_id() == (component::LoRa::component_id_base + 0) &&
                packet.packet_id() == component::LoRa::send_command_id) {
                continue;
            }

            if (packet.find("Ss")) {
                // LoRa already published this packet to SerialBus's independent
                // listener. Publishing it again here would duplicate PC delivery.
                // This branch accepts both ACK 'a' and telemetry 'M' (and other IDs).
                logDownlink(packet.packet_id());
                continue;
            }

            // Local GPS/power telemetry and LOG packets must not occupy the RF
            // channel or turn each downlink log into a new uplink transmission.
            if (!packet.isCommand()) continue;

            auto im = packet.find("Im");
            if (im) {
                continue;
            }

            if (packet.size() > component::LoRa::max_packet_size) {
                LOG("GOLIDEN uplink rejected: packet too large (%u)", packet.size());
                continue;
            }

            // Admission with backpressure avoids dropping PC commands while busy.
            // Direct queueing does not re-publish to tx_listener_ or SerialBus.
            if (!lora.queuePacket(packet, component::LoRa::TxPriority::Normal)) {
                pending_tx_ = packet;
                return;
            }
        }
    }

private:
    void logDownlink(uint8_t id) {
        // Same '#' / 'Ms' log format as LOG(), with explicit listener exclusion.
        char message[80];
        snprintf(message, sizeof(message),
                 "[GOLIDEN] LoRa Rx -> SerialBus Tx (Packet ID: %c)", id);
        wcpp::Packet trace = newPacket(96);
        if (!trace) return;
        trace.telemetry(packet_id_log, component_id());
        trace.append("Ms").setString(message);
        sendPacket(trace, tx_listener_);
    }
} main_;

void setup() {
    Serial.begin(115200);
    kernel::setUnitId(unit_id);
    if (!kernel::begin(module_id, true)) return;

    //Serial0.setPins(4, 5);
    Wire.begin(17, 16);  // SDA=17, SCL=16
    serial_bus.begin();

    SPI.begin(SDCARD_SCK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN, SDCARD_SS_PIN);

    delay(1000); 

    status_indicator.begin();
    status_indicator.blink_on_change();

    error_indicator.begin();
    error_indicator.set(true);

    power.begin();
    lora.begin();
    //pressure.begin();
    //imu.begin();
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
