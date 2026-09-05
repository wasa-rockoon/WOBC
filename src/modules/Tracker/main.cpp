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

    void setup() override {
        uplink_listener_.command();
        listen(uplink_listener_, 32);
        gps_listener_.telemetry().component(component::GPS::component_id)
            .unit_origin(unit_id).packet(component::GPS::telemetry_id);
        pressure_listener_.telemetry().component(component::Pressure::component_id)
            .unit_origin(unit_id).packet(component::Pressure::telemetry_id);
        listen(gps_listener_, 1);
        listen(pressure_listener_, 1);
        last_telemetry_ms_ = millis();
    }

    void loop() override {
        // Retain a received command until its ACK has entered the priority queue.
        // queuePacket() bypasses the lossy generic command listener, and returns
        // false on backpressure. No packet is re-published into our own listener.
        for (;;) {
            if (pending_uplink_) {
                wcpp::Packet ack = newPacket(32);
                if (!ack) return;
                ack.telemetry('a', pending_uplink_.component_id());
                ack.append("St").setInt(0);
                ack.append("Ri").setInt(pending_uplink_.packet_id());
                if (!lora.queuePacket(ack, component::LoRa::TxPriority::Ack)) return;
                LOG("[Tracker] Uplink received & ACK sending... (Packet ID: %c)",
                    pending_uplink_.packet_id());
                pending_uplink_ = wcpp::Packet::null();
                last_telemetry_ms_ = millis();
            }
            if (!uplink_listener_) break;
            wcpp::Packet packet = uplink_listener_.pop();
            if (!packet.find("Ss")) continue;
            if (packet.packet_id() != 't' && packet.packet_id() != 'c') continue;
            pending_uplink_ = packet;
        }

        if (gps_listener_) {
            latest_gps_ = gps_listener_.pop();
            gps_ms_ = millis();
        }
        if (pressure_listener_) {
            latest_pressure_ = pressure_listener_.pop();
            pressure_ms_ = millis();
        }
        const uint32_t now = millis();
        if (uint32_t(now - last_telemetry_ms_) < telemetry_interval_ms ||
            uplink_listener_ || !lora.canSendTelemetry()) return;

        wcpp::Packet telemetry = newPacket(64);
        if (!telemetry) return;
        telemetry.telemetry('M', component_id(), unit_id, 0xFF, telemetry_sequence_);
        telemetry.append("Ts").setInt(now);
        // Cache sensor packets; never call sensor/UART drivers from this task.
        // Missing/old samples are omitted instead of inventing a zero value.
        if (latest_gps_ && uint32_t(now - gps_ms_) < 5000) {
            for (const char* name : {"LA", "LO"}) {
                auto value = latest_gps_.find(name);
                if (value) telemetry.append(name).setFloat32((*value).getFloat32());
            }
            auto altitude = latest_gps_.find("AL");
            if (altitude) telemetry.append("AL").setInt((*altitude).getInt());
        }
        if (latest_pressure_ && uint32_t(now - pressure_ms_) < 5000) {
            for (const char* name : {"PR", "PA"}) {
                auto value = latest_pressure_.find(name);
                if (value) telemetry.append(name).setInt((*value).getInt());
            }
        }
        // This second check/admission is serialized with ACK queueing and TX.
        if (!lora.queuePacket(telemetry, component::LoRa::TxPriority::Telemetry)) return;
        last_telemetry_ms_ = now; // No catch-up burst after busy periods.
        ++telemetry_sequence_;
    }

private:
    static constexpr uint32_t telemetry_interval_ms = 2000;
    kernel::Listener uplink_listener_, gps_listener_, pressure_listener_;
    wcpp::Packet pending_uplink_ = wcpp::Packet::null();
    wcpp::Packet latest_gps_ = wcpp::Packet::null();
    wcpp::Packet latest_pressure_ = wcpp::Packet::null();
    uint32_t last_telemetry_ms_ = 0, gps_ms_ = 0, pressure_ms_ = 0;
    uint16_t telemetry_sequence_ = 0;
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
    lora.enableTrackerScheduling();
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
