// #define NDEBUG

#include <library/wobc.h>
#include <components/Pressure/pressure.h>
#include <components/Logger/logger.h>
#include <components/IGN/IGN.h>
#include <components/Heater/Heater.h>
#include <components/FlightPin/FlightPin.h>
#include <components/Telemeter/telemeter.h>
#include <SPI.h>
#include <driver/gpio.h>
#include <esp_intr_alloc.h>

#define SPI0_SCK_PIN 12
#define SPI0_MOSI_PIN 11
#define SPI0_MISO_PIN 13
#define SPI0_CS_PIN 10

#define SD_INSERTED_PIN 9
#define SDCARD_MOSI_PIN SPI0_MOSI_PIN
#define SDCARD_MISO_PIN SPI0_MISO_PIN
#define SDCARD_SS_PIN SPI0_CS_PIN
#define SDCARD_SCK_PIN SPI0_SCK_PIN

constexpr uint8_t module_id = 'I';
constexpr uint8_t unit_id = 0x40;
constexpr int ign_normal_pin = 6;
constexpr int ign_high_pin = 4;
constexpr int ign_low_pin = 5;
constexpr int flight_pin_pin = 21;
constexpr int32_t ignition_altitude_m = 15000;
constexpr component::Heater::AdcResolution heater_adc_resolution =
    component::Heater::AdcResolution::BIT_16;

core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);

component::Logger logger(SPI, SPI0_CS_PIN, SD_INSERTED_PIN);
component::Pressure pressure(Wire, unit_id);
component::IGN ign(Wire, ign_normal_pin, ign_high_pin, ign_low_pin, unit_id,
                   ignition_altitude_m, 1);
component::Heater heater(Wire, unit_id, 2, component::Heater::HEATER_PIN, heater_adc_resolution);
component::FlightPin flight_pin(unit_id, flight_pin_pin, 1);
component::Telemeter telemeter;

void IRAM_ATTR onFlightPinInserted(void*) {
    ign.abortSequenceFromISR();
}

bool beginFlightPinAbortInterrupt() {
    pinMode(flight_pin_pin, INPUT);

    const esp_err_t install_result = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    // 既存サービスのIRAM属性は確認できないため、既に導入済みの場合も安全側で失敗とする。
    if (install_result != ESP_OK) {
        return false;
    }

    if (gpio_set_intr_type(static_cast<gpio_num_t>(flight_pin_pin), GPIO_INTR_POSEDGE)
        != ESP_OK) {
        return false;
    }

    return gpio_isr_handler_add(static_cast<gpio_num_t>(flight_pin_pin),
                                onFlightPinInserted, nullptr) == ESP_OK;
}

interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

class Main : public process::Component {
public:
    Main() : process::Component("main", 0x00) {}
    kernel::Listener flight_pin_listener_;

    void setup() override {
        flight_pin_listener_.telemetry()
                            .packet(component::FlightPin::telemetry_id)
                            .component(component::FlightPin::component_id)
                            .unit_origin(unit_id);
        listen(flight_pin_listener_, 4);
    }

    void loop() override {
        while (flight_pin_listener_) {
            const wcpp::Packet packet = flight_pin_listener_.pop();
            auto fp = packet.find(component::FlightPin::stateEntryName());
            if (!fp || !(*fp).isInt()) {
                flight_pin_removed_ = false;
                ign.altitudeConditionMet(false);
                continue;
            }

            const int64_t state = (*fp).getInt();
            if (state != LOW && state != HIGH) {
                flight_pin_removed_ = false;
                ign.altitudeConditionMet(false);
                continue;
            }

            if (state == HIGH) {
                flight_pin_inserted_seen_ = true;
                flight_pin_removed_ = false;
                ign.altitudeConditionMet(false);
                continue;
            }

            flight_pin_removed_ = flight_pin_inserted_seen_;
        }

        const bool removed = flight_pin_removed_ && digitalRead(flight_pin_pin) == LOW;
        const bool altitude_ready = ign.altitudeConditionMet(removed);
        if (removed && altitude_ready && !ignition_start_requested_) {
            ignition_start_requested_ = true;
            if (ign.startSequence()) {
                LOG("Flight pin removed and altitude confirmed 30 times; ignition sequence requested");

                // 挿入が開始要求と割り込みのアームの境界で発生した場合も、
                // 現在値を確認して取りこぼさず中止する。
                if (digitalRead(flight_pin_pin) == HIGH) {
                    ign.abortSequence();
                    LOG("Flight pin inserted during ignition sequence start; aborted");
                }
            } else {
                LOG("Flight pin and altitude conditions met; ignition sequence request rejected");
            }
        }
    }

private:
    bool flight_pin_inserted_seen_ = false;
    bool flight_pin_removed_ = false;
    bool ignition_start_requested_ = false;
} main_;

void setup() {
    Serial.begin(115200);

    // Keep the ignition outputs safe even if kernel or task startup fails.
    if (!ign.prepareSafeOutputs()) return;

    // Allow the externally supplied 3.3 V rail and peripherals to settle
    // before starting the kernel and CAN tasks. Ignition outputs stay LOW.
    delay(1000);

    kernel::setUnitId(unit_id);
    if (!kernel::begin(module_id, true)) return;

    Serial0.setPins(2, 1);
    if (!Wire.begin(17, 16)) return;

    can_bus.begin();
    serial_bus.begin();

    SPI.begin(SDCARD_SCK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN, SDCARD_SS_PIN);

    delay(1000); 

    status_indicator.begin();
    status_indicator.blink_on_change();

    error_indicator.begin();
    error_indicator.set(true);

    pressure.begin();
    logger.begin();
    heater.begin();
    telemeter.begin();

    // Initialize IGN without starting the sequence. It remains Disarmed until
    // FlightPin removal AND 30 consecutive pressure altitude samples qualify.
    if (!ign.begin(false)) return;
    if (!beginFlightPinAbortInterrupt()) return;
    main_.begin();
    flight_pin.begin();

    error_indicator.set(false);
    error_indicator.blink_on_change(100);
}

void loop() {
    status_indicator.update();
    error_indicator.update();
}
