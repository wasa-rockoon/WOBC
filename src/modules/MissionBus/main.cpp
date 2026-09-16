// #define NDEBUG

#include <library/wobc.h>
#include <components/LiPoPower/lipo_power.h>
#include <components/LoRa/lora.h>
#include <components/Pressure/pressure.h>
#include <components/GPS/gps.h>
#include <components/Logger/logger.h>
#include <components/IMU/IMU.h>
#include <SPI.h>

#define SPI0_SCK_PIN 5
#define SPI0_MOSI_PIN 1
#define SPI0_MISO_PIN 2
#define SPI0_CS_PIN 4

#define SD_INSERTED_PIN 6
#define SDCARD_MOSI_PIN SPI0_MOSI_PIN
#define SDCARD_MISO_PIN SPI0_MISO_PIN
#define SDCARD_SS_PIN SPI0_CS_PIN
#define SDCARD_SCK_PIN SPI0_SCK_PIN

#define ST 8
#define PG 11
#define STAT1 10
#define STAT2 -1
#define HEAT 48
#define CHARGELED -1
#define TEMP 9

#define LORA_CHANNEL 11
#define LORA_TX_PIN 13
#define LORA_RX_PIN 12
#define LORA_AUX_PIN 21
#define LORA_M0_PIN 14
#define LORA_M1_PIN 18

// GS-compatible setting; verify against the MissionBus board wiring.
#define CAN_RX_PIN 44
#define CAN_TX_PIN 43

constexpr uint8_t module_id = 0x4D;
constexpr uint8_t unit_id = 0x62;
constexpr uint8_t ign_unit_id = 0x40;
// false: binary SerialBus output for the telemetry viewer.
// true: readable CAN diagnostics for a plain serial monitor (viewer disabled).
constexpr bool can_serial_monitor = false;

HardwareSerial lora_serial(1);
core::CANBus can_bus(CAN_RX_PIN, CAN_TX_PIN);
core::SerialBus serial_bus(Serial);

component::LiPoPower power(Wire, ST, PG, STAT1, STAT2, HEAT, CHARGELED, TEMP, unit_id, 1);
component::LoRa lora(LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN, LORA_TX_PIN, LORA_RX_PIN, LORA_CHANNEL, 0);
component::Logger logger(SPI, SPI0_CS_PIN, SD_INSERTED_PIN);
component::Pressure pressure(Wire, unit_id);
component::IMU9 imu(Wire, unit_id, 10, IMU_DATA, IMU_ICM_MMC);
component::GPS gps(38, 39, 115200, unit_id);

interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

class Main : public process::Component {
public:
    Main() : process::Component("main", 0x00) {}
    kernel::Listener my_listener_;
    kernel::Listener heartbeat_;
    kernel::Listener ign_listener_;

    void setup() override {
        my_listener_.telemetry(); 
        listen(my_listener_, 8);
        heartbeat_.component(0x4D);
        listen(heartbeat_,1);
        if (can_serial_monitor) {
            // Include commands so older IGN firmware with a missing type bit
            // remains visible in the monitor without reclassifying its data.
            ign_listener_.unit_origin(ign_unit_id);
            listen(ign_listener_, 16);
        }
    }

    void loop() override {
        while (ign_listener_) {
            const wcpp::Packet packet = ign_listener_.pop();
            ++ign_rx_count_;
            Serial.printf("[CAN RX] IGN unit=0x40 type=%s component=0x%02X packet=%c bytes=%u count=%lu",
                          packet.isTelemetry() ? "TLM" : "CMD",
                          (unsigned)packet.component_id(), (int)packet.packet_id(),
                          (unsigned)packet.size(), ign_rx_count_);
            const char* fields[] = {"Vi", "Ii", "Pi"};
            for (const char* name : fields) {
                auto entry = packet.find(name);
                if (entry && (*entry).isInt()) {
                    Serial.printf(" %s=%lld", name, (long long)(*entry).getInt());
                }
            }
            Serial.println();
        }
        while (my_listener_) {
            const wcpp::Packet packet = my_listener_.pop();
            auto im = packet.find("Im");
            if (!im) {
                wcpp::Packet lorapacket = newPacket(64);
                if (!lorapacket) continue;
                lorapacket.command(lora.send_command_id, lora.component_id_base + 0);
                lorapacket.append("Pa").setPacket(packet);
                sendPacket(lorapacket);
            }
        }
        if (can_serial_monitor && millis() - last_can_report_ms_ >= 5000) {
            last_can_report_ms_ = millis();
            Serial.printf("[CAN MONITOR] RX=%d TX=%d baud=%u IGN packets=%lu\n",
                          CAN_RX_PIN, CAN_TX_PIN, (unsigned)WOBC_CAN_BUS_BAUDRATE,
                          ign_rx_count_);
        }
    }

private:
    unsigned long ign_rx_count_ = 0;
    unsigned long last_can_report_ms_ = 0;
} main_;

void setup() {
    Serial.begin(115200);
    kernel::setUnitId(unit_id); //GS
    if (!kernel::begin(module_id, true)) return;

    Serial0.setPins(4, 5);
    Wire.begin(17, 16);
    if (!can_serial_monitor) serial_bus.begin();

    SPI.begin(SDCARD_SCK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN, SDCARD_SS_PIN);

    delay(1000); 

    status_indicator.begin();
    status_indicator.blink_on_change();

    error_indicator.begin();
    error_indicator.set(true);

    can_bus.begin();
    power.begin();
    lora.begin();
    pressure.begin();
    imu.begin();
    gps.begin();
    main_.begin();

    error_indicator.set(false);
    error_indicator.blink_on_change(100);
}

void loop() {
    status_indicator.update();
    error_indicator.update();
}
