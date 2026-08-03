#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <library/wobc.h>
#include <components/Logger/logger.h>
#include <components/IMU/IMU.h>
#include <components/Pressure/pressure.h>
#include <components/Heater/Heater.h>
#include <components/Servo/LogServo.h>

#define I2C_SCL_PIN 5
#define I2C_SDA_PIN 4
#define I2C_freq 1000000

#define SPI0_SCK_PIN 18
#define SPI0_MOSI_PIN 15
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

component::Logger logger(SPI, SPI0_CS_PIN, -1, 10.0);
component::IMU9 imu(Wire, unit_id, 100, IMU_DATA_WITH_KALMAN_6, IMU_ICM_MMC);
component::Heater heater(Wire, unit_id, 1);
component::LogServo servo(12, 34, unit_id, WITH_READANGLE, 50);
//component::Pressure pressure(Wire, unit_id, 20);

interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

class Main : public process::Component {
public:
    Main() : process::Component("Main", 0x00) {}
    kernel::Listener my_listener_;
    kernel::Listener heartbeat_;
  
    void setup() override {
        LOG("CMG Task: Setup started"); 
        my_listener_.telemetry(); 
        listen(my_listener_, 8);
        heartbeat_.component(0x4D);
        listen(heartbeat_,1);
        LOG("CMG Task: loop starts"); 
    }

    void loop() override {
        static uint32_t start_time = millis();
        if (millis() - start_time > 1200000) {
            request_file_split.store(true);
            start_time = millis();
        }
        delay(100);
        // while (my_listener_) { //main向けのログがあれば実行
            float Ax = 0.1;
            float Ay = 0.2;
            float Az = 0.3;
            float Gx = 0.4;
            float Gy = 0.5;
            float Gz = 0.6;
            float Mx = 0.7;
            float My = 0.8;
            float Mz = 0.9;
            wcpp::Packet packet = my_listener_.pop();
            wcpp::Packet new_packet = newPacket(128);
            new_packet.telemetry('C', component_id(), unit_id, 0xFF, 1234);
            new_packet.append("Ts").setInt((int)millis());
            new_packet.append("Ax").setFloat32(Ax);
            new_packet.append("Ay").setFloat32(Ay);
            new_packet.append("Az").setFloat32(Az);
            new_packet.append("Gx").setFloat32(Gx);
            new_packet.append("Gy").setFloat32(Gy);
            new_packet.append("Gz").setFloat32(Gz);
            new_packet.append("Mx").setFloat32(Mx);
            new_packet.append("My").setFloat32(My);
            new_packet.append("Mz").setFloat32(Mz);

            sendPacket(new_packet);

            //}
        }
} main_;

void setup() {
    Serial.begin(115200);
    kernel::setUnitId(unit_id);
    if (!kernel::begin(module_id, true)) return;

    wobc::beginSPI(SPI, SDCARD_SCK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN, SDCARD_SS_PIN);
    wobc::beginI2C(Wire, I2C_SDA_PIN, I2C_SCL_PIN, I2C_freq);
    serial_bus.begin(); 

    delay(1000);

    status_indicator.begin();
    status_indicator.blink_on_change();

    error_indicator.begin();
    error_indicator.set(true);
    
    logger.begin();
    imu.begin(); 
    heater.begin();
    servo.begin();
    //pressure.begin();
    main_.begin();

    error_indicator.set(false);
    error_indicator.blink_on_change(100);
}

void loop() {
    status_indicator.update();
    error_indicator.update();
}