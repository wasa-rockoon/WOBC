// #define NDEBUG

#include <library/wobc.h>
#include <components/Pressure/pressure.h>
#include <components/Logger/logger.h>
#include <components/FlightPin/FlightPin.h>
#include <components/GPS/gps.h>
#include <components/LiPoPower/lipo_power.h>
//#include <components/Telemeter/telemeter.h>
#include <components/Nichrome/Nichrome.h>
#include <components/Nichrome/NichromeStartGate.h>
#include <components/Separation/separation_ack.h>
#include <driver/gpio.h>
#include <esp_intr_alloc.h>
#include <SPI.h>

// TODO: Confirm the Separation board pin assignments; these follow IGN.
#define SPI0_SCK_PIN 12
#define SPI0_MOSI_PIN 13
#define SPI0_MISO_PIN 11
#define SPI0_CS_PIN 9

#define SD_INSERTED_PIN 10
#define SDCARD_MOSI_PIN SPI0_MOSI_PIN
#define SDCARD_MISO_PIN SPI0_MISO_PIN
#define SDCARD_SS_PIN SPI0_CS_PIN
#define SDCARD_SCK_PIN SPI0_SCK_PIN

#define ST 5
#define PG 4
#define STAT1 6
#define STAT2 -1
#define HEAT -1
#define CHARGELED -1
#define TEMP -1

constexpr uint8_t module_id = 'S';
// TODO: Assign a unique Separation unit ID before use alongside IGN.
constexpr uint8_t unit_id = 0x41;
static_assert(unit_id == component::SeparationAckProtocol::separation_unit_id,
              "Update the communication test protocol when changing the unit ID");
constexpr int nichrome_normal_pin = 14;
constexpr int nichrome_high_pin = 48;
constexpr int nichrome_low_pin = 47;
constexpr int flight_pin_pin = 2;
constexpr int32_t separation_altitude_m = 20000;
// Fallback only when valid pressure updates have stopped for 60 seconds.
constexpr uint32_t separation_fallback_delay_ms = 72UL * 60UL * 1000UL;

core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);

component::Logger logger(SPI, SPI0_CS_PIN, SD_INSERTED_PIN);
component::Pressure pressure(Wire, unit_id);
component::FlightPin flight_pin(unit_id, flight_pin_pin, 1);
//component::Telemeter telemeter;
component::Nichrome nichrome(Wire, nichrome_normal_pin, nichrome_high_pin, nichrome_low_pin, unit_id, 1);
component::LiPoPower power(Wire, ST, PG, STAT1, STAT2, HEAT, CHARGELED, TEMP, unit_id, 1);
component::GPS gps(38, 39, 115200, unit_id);
component::SeparationAck separation_ack;

void IRAM_ATTR onFlightPinInserted(void*) {
    nichrome.abortSequenceFromISR();
}

bool beginFlightPinAbortInterrupt() {
    pinMode(flight_pin_pin, INPUT);
    // Require an IRAM ISR service, as in IGN; an existing service is not accepted.
    if (gpio_install_isr_service(ESP_INTR_FLAG_IRAM) != ESP_OK) return false;
    if (gpio_set_intr_type(static_cast<gpio_num_t>(flight_pin_pin), GPIO_INTR_POSEDGE)
        != ESP_OK) return false;
    return gpio_isr_handler_add(static_cast<gpio_num_t>(flight_pin_pin),
                                onFlightPinInserted, nullptr) == ESP_OK;
}

interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

class Main : public process::Component {
public:
    Main() : process::Component("main", 0x00) {}

    void setup() override {
        flight_pin_listener_.telemetry()
                            .packet(component::FlightPin::telemetry_id)
                            .component(component::FlightPin::component_id)
                            .unit_origin(unit_id);
        listen(flight_pin_listener_, 4);
        pressure_listener_.telemetry()
                          .packet(component::Pressure::telemetry_id)
                          .component(component::Pressure::component_id)
                          .unit_origin(unit_id);
        listen(pressure_listener_, 4);
    }

    void loop() override {
        while (flight_pin_listener_) {
            const wcpp::Packet packet = flight_pin_listener_.pop();
            const auto fp = packet.find(component::FlightPin::stateEntryName());
            const bool valid = fp && (*fp).isInt();
            start_gate_.observeFlightPin(valid, valid ? (*fp).getInt() : 0);
        }

        if (start_gate_.updateFlightPin(digitalRead(flight_pin_pin) == LOW, millis())) {
            while (pressure_listener_) pressure_listener_.pop();
            return;
        }
        while (pressure_listener_) {
            const wcpp::Packet packet = pressure_listener_.pop();
            const auto source = packet.find("Sm");
            if (!source || !(*source).isInt() || (*source).getInt() != kernel::module_id()) continue;
            const auto pa = packet.find("PA");
            const auto validity = packet.find("Va");
            const auto ts = packet.find("Ts");
            const bool valid = pa && (*pa).isInt()
                && validity && (*validity).isInt() && (*validity).getInt() == 1
                && ts && (*ts).isInt() && (*ts).getInt() >= 0 && (*ts).getInt() <= UINT32_MAX;
            start_gate_.observePressure(valid, valid ? (*pa).getInt() : 0,
                valid ? static_cast<uint32_t>((*ts).getInt()) : 0, millis());
        }
        const bool ready = start_gate_.ready(millis(), separation_fallback_delay_ms)
                        && digitalRead(flight_pin_pin) == LOW;
        if (separation_start_attempted_ || !ready) return;

        separation_start_attempted_ = true;
        if (nichrome.startSequence()) {
            LOG("Flight pin removed and separation start conditions met; sequence requested");
            // Cover insertion between the start request and ISR arming.
            if (digitalRead(flight_pin_pin) == HIGH) {
                nichrome.abortSequence();
                LOG("Flight pin inserted during separation sequence start; aborted");
            }
        } else {
            LOG("Separation sequence start rejected");
        }
    }

private:
    kernel::Listener flight_pin_listener_;
    kernel::Listener pressure_listener_;
    component::NichromeStartGate start_gate_{separation_altitude_m};
    bool separation_start_attempted_ = false;
} main_;

void setup() {
    Serial.begin(115200);

    if (!nichrome.prepareSafeOutputs()) return;

    // Allow the power rail and peripherals to settle before starting tasks.
    delay(1000);

    kernel::setUnitId(unit_id);
    if (!kernel::begin(module_id, true)) return;

    Serial0.setPins(2, 1);
    if (!Wire.begin(17, 16)) return;

    can_bus.begin();
    serial_bus.begin();
    if (!separation_ack.begin()) return;

    SPI.begin(SDCARD_SCK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN, SDCARD_SS_PIN);

    delay(1000);

    status_indicator.begin();
    status_indicator.blink_on_change();

    error_indicator.begin();
    error_indicator.set(true);

    pressure.begin();
    logger.begin();
    power.begin();
    gps.begin();

    if (!nichrome.begin(false)) return;
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
