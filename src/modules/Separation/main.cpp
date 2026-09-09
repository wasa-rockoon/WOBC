// #define NDEBUG

#include <library/wobc.h>
#include <components/Pressure/pressure.h>
#include <components/Logger/logger.h>
#include <components/FlightPin/FlightPin.h>
#include <components/GPS/gps.h>
#include <components/LiPoPower/lipo_power.h>
//#include <components/Telemeter/telemeter.h>
#include <components/Separation/Separation.h>
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

constexpr uint8_t module_id = 'S';
// TODO: Assign a unique Separation unit ID before use alongside IGN.
constexpr uint8_t unit_id = 0x41;
constexpr int flight_pin_pin = 2;

core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);

component::Logger logger(SPI, SPI0_CS_PIN, SD_INSERTED_PIN);
component::Pressure pressure(Wire, unit_id);
component::FlightPin flight_pin(unit_id, flight_pin_pin, 1);
//component::Telemeter telemeter;
component::Separation separation(Wire, unit_id, 1);

interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

class Main : public process::Component {
public:
    Main() : process::Component("main", 0x00) {}

    void setup() override {
        // TODO: Register listeners needed by the separation sequence.
    }

    void loop() override {
        // TODO: Implement the Separation control logic.
    }
} main_;

void setup() {
    Serial.begin(115200);

    // Allow the power rail and peripherals to settle before starting tasks.
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

    separation.begin(false);

    main_.begin();
    flight_pin.begin();


    error_indicator.set(false);
    error_indicator.blink_on_change(100);
}

void loop() {
    status_indicator.update();
    error_indicator.update();
}
