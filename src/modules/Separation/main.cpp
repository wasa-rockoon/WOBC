// #define NDEBUG

#include <library/wobc.h>
#include <components/Pressure/pressure.h>
#include <components/Logger/logger.h>
#include <components/FlightPin/FlightPin.h>
#include <components/GPS/gps.h>
#include <components/LiPoPower/lipo_power.h>
//#include <components/Telemeter/telemeter.h>
#include <components/Nichrome/Nichrome.h>
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
constexpr int nichrome_normal_pin = 14;
constexpr int nichrome_high_pin = 48;
constexpr int nichrome_low_pin = 47;
constexpr int flight_pin_pin = 2;
// Start the sequence once, 90 minutes after boot (millis() origin).
constexpr unsigned long separation_start_delay_ms = 90UL * 60UL * 1000UL;

core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);

component::Logger logger(SPI, SPI0_CS_PIN, SD_INSERTED_PIN);
component::Pressure pressure(Wire, unit_id);
component::FlightPin flight_pin(unit_id, flight_pin_pin, 1);
//component::Telemeter telemeter;
component::Nichrome nichrome(Wire, nichrome_normal_pin, nichrome_high_pin, nichrome_low_pin, unit_id, 1);
component::LiPoPower power(Wire, ST, PG, STAT1, STAT2, HEAT, CHARGELED, TEMP, unit_id, 1);


interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

class Main : public process::Component {
public:
    Main() : process::Component("main", 0x00) {}

    void setup() override {
        // TODO: Register listeners needed by the separation sequence.
    }

    void loop() override {
        if (separation_start_attempted_ || millis() < separation_start_delay_ms) return;

        separation_start_attempted_ = true;
        if (nichrome.startSequence()) {
            LOG("Separation sequence started after 90 minutes from boot");
        } else {
            LOG("Separation sequence start rejected");
        }
    }

private:
    bool separation_start_attempted_ = false;
} main_;

void setup() {
    Serial.begin(115200);

    // Allow the power rail and peripherals to settle before starting tasks.
    delay(1000);

    kernel::setUnitId(unit_id);
    if (!kernel::begin(module_id, false)) return;

    Serial0.setPins(2, 1);
    if (!Wire.begin(17, 16)) return;

    //can_bus.begin();
    serial_bus.begin();

    SPI.begin(SDCARD_SCK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN, SDCARD_SS_PIN);

    delay(1000);

    status_indicator.begin();
    status_indicator.blink_on_change();

    error_indicator.begin();
    error_indicator.set(true);

    pressure.begin();
    logger.begin();
    power.begin();

    nichrome.begin(false);

    main_.begin();
    flight_pin.begin();


    error_indicator.set(false);
    error_indicator.blink_on_change(100);
}

void loop() {
    status_indicator.update();
    error_indicator.update();
}
