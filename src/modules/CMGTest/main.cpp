#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <library/wobc.h>
#include <components/Logger/logger.h>
#include <components/IMU/IMU.h>

#define I2C_SCL_PIN 5
#define I2C_SDA_PIN 4
#define I2C_freq 400000

#define SPI0_SCK_PIN 18
#define SPI0_MOSI_PIN 19
#define SPI0_MISO_PIN 16
#define SPI0_CS_PIN 17

#define SD_INSERTED_PIN 9
#define SDCARD_MOSI_PIN SPI0_MOSI_PIN
#define SDCARD_MISO_PIN SPI0_MISO_PIN
#define SDCARD_SS_PIN SPI0_CS_PIN
#define SDCARD_SCK_PIN SPI0_SCK_PIN

core::SerialBus serial_bus(Serial); 

constexpr uint8_t module_id = 0x45;
constexpr uint8_t unit_id = 0x66;

component::Logger logger(SPI, SPI0_CS_PIN, -1);
component::IMU9 imu(Wire, unit_id, 100);

class Main : public process::Component {
public:
    Main() : process::Component("main", 0x00) {}

    void setup() override {
        LOG("CMG Task: Setup started"); // Serial.printlnの代わりにこれを使う
    }

    void loop() override {
        delay(10); // 100ms周期 (10Hz)

        // 0最適化バグ回避のため微小な値を入れる
        float qx = 0.00001, qy = 0.00001, qz = 0.00001, qw = 1.0;

        wcpp::Packet log_packet = newPacket(64);
        log_packet.telemetry('C', component_id());
        
        log_packet.append("Ts").setInt(millis());
        log_packet.append("Qx").setFloat32(qx); 
        log_packet.append("Qy").setFloat32(qy);
        log_packet.append("Qz").setFloat32(qz);
        log_packet.append("Qw").setFloat32(qw);
        
        sendPacket(log_packet);
    }
} main_;

void setup() {
    Serial.begin(115200);

    wobc::beginSPI(SPI, SDCARD_SCK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN, SDCARD_SS_PIN);
    wobc::beginI2C(Wire, I2C_SDA_PIN, I2C_SCL_PIN, I2C_freq);

    kernel::setUnitId(unit_id);
    if (!kernel::begin(module_id, true)) {
        return; 
    }

    serial_bus.begin(); 
    logger.begin();
    //imu.begin(); 
    main_.begin();
}

void loop() {
    delay(1000);
}