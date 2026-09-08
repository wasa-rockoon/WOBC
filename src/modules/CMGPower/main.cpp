#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <library/wobc.h>
#include <components/Logger/logger.h>
#include <components/Pressure/pressure.h>
#include <components/Heater/Heater.h>
#include <components/PowerMeasure/PowerMeasure.h>
#include <components/MotorControl/MotorControl.h>
#include <components/Servo/LogServo.h>

#define I2C_SCL_PIN 16
#define I2C_SDA_PIN 17
#define I2C_freq 400000

#define SPI0_SCK_PIN 12
#define SPI0_MOSI_PIN 13
#define SPI0_MISO_PIN 11
#define SPI0_CS_PIN 14

#define SD_INSERTED_PIN 21
#define SDCARD_MOSI_PIN SPI0_MOSI_PIN
#define SDCARD_MISO_PIN SPI0_MISO_PIN
#define SDCARD_SS_PIN SPI0_CS_PIN
#define SDCARD_SCK_PIN SPI0_SCK_PIN

#define SERVO_SIG_PIN 10
#define SERVO_READ_PIN 9

#define TACHOMETER_CH1_PIN 18
#define TACHOMETER_CH2_PIN 8

#define MOTOR_CH1_1_PIN 4
#define MOTOR_CH1_2_PIN 5
#define MOTOR_CH2_1_PIN 6
#define MOTOR_CH2_2_PIN 7

#define SERVO_EN_PIN 47
#define HEATER_PIN 40


core::SerialBus serial_bus(Serial); 
core::CANBus can_bus(44, 43); // CAN RX, CAN TX

constexpr uint8_t module_id = 0x46;
constexpr uint8_t unit_id = 0x66;

component::Logger logger(SPI, SPI0_CS_PIN, SD_INSERTED_PIN, 10.0);
component::Heater heater(Wire, unit_id, 1);
component::Pressure pressure(Wire, unit_id, 1);
component::PowerMeasure power_measure(Wire, unit_id, 10);
component::MotorControl motor(TACHOMETER_CH1_PIN, TACHOMETER_CH2_PIN, unit_id, 1000, 50);
component::LogServo servo(SERVO_SIG_PIN, SERVO_READ_PIN, unit_id, WITH_READANGLE, 50);

interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

uint8_t isMotorOn = 0;
bool motor_started = false;
uint8_t highAltitude = 0;
uint8_t overheating = 0;
static uint32_t missionStartTime = 0;
static uint32_t fileSplitTime = 0;
static uint32_t motorStartTime = 0;

class Main : public process::Component {
public:
    Main() : process::Component("Main", 0x00) {}
    kernel::Listener pressure_listener_;
    kernel::Listener heater_listener_;
  
    void setup() override {
        LOG("CMG Task: Setup started"); 
        pressure_listener_.component(0x25);
        listen(pressure_listener_, 128);
        heater_listener_.component(0x46);
        listen(heater_listener_, 128);
        LOG("CMG Task: loop starts"); 
        missionStartTime = millis();
        fileSplitTime = missionStartTime;
    }

    void loop() override {
        if (millis() - fileSplitTime > 1200000) {
            request_file_split.store(true);
            fileSplitTime = millis();
        }

        delay(20);

        while (pressure_listener_) {
            wcpp::Packet packet = pressure_listener_.pop();
            auto e = packet.find("PA");
            if (e) {
                int pa = (*e).getInt();
                if (pa > 17000) { // 高高度の判定条件
                    highAltitude++;
                }
            }
        }

        while (heater_listener_) {
            wcpp::Packet packet = heater_listener_.pop();
            auto e = packet.find("Ca");
            if (isMotorOn && e && (*e).getFloat16() > 50.0f) { // 高温度の判定条件
                overheating++;
            }
        }

        if (!motor_started &&
            (highAltitude > 19 || millis() - missionStartTime > 1800000)) { // 高高度が20回以上検出された場合、または30分経過した場合
            motor_started = true;
            isMotorOn = 1;
            motorStartTime = millis();
            digitalWrite(SERVO_EN_PIN, HIGH); // サーボモータの電源をONにする
            highAltitude = 0; // カウンタをリセット
            request_file_split.store(true);
            LOG("Motor activated.");
        }

        if (isMotorOn &&
            (overheating > 4 || millis() - motorStartTime > 300000)) { // 高温度が5回以上検出された場合、または5分経過した場合
            isMotorOn = 0; 
            digitalWrite(SERVO_EN_PIN, LOW); // サーボモータの電源をOFFにする
            overheating = 0; // カウンタをリセット
            request_file_split.store(true);
            LOG("Motor deactivated.");
        }

        wcpp::Packet command_packet = newPacket(48);
        command_packet.command('C', component_id(), unit_id, 0xFF, 1234);
        command_packet.append("St").setInt(isMotorOn);  
        command_packet.append("An").setFloat32(60.0f); 
        sendPacket(command_packet);
        }
} main_;

void setup() {
    Serial.begin(115200);
    kernel::setUnitId(unit_id);
    if (!kernel::begin(module_id, false)) return;

    //wobc::beginSPI(SPI, SDCARD_SCK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN, SDCARD_SS_PIN);
    wobc::beginI2C(Wire, I2C_SDA_PIN, I2C_SCL_PIN, I2C_freq);
    serial_bus.begin(); 
    can_bus.begin();

    //Servo servo1;

    pinMode(SERVO_EN_PIN, OUTPUT);

    delay(1000);

    status_indicator.begin();
    status_indicator.blink_on_change();

    error_indicator.begin();
    error_indicator.set(true);
    
    logger.begin(); 
    heater.begin();
    pressure.begin();
    power_measure.begin();
    motor.begin();
    servo.begin();
    main_.begin();

    error_indicator.set(false);
    error_indicator.blink_on_change(100);
}

void loop() {
    status_indicator.update();
    error_indicator.update();
}