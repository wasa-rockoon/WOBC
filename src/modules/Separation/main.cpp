#include <library/wobc.h>
#include <SPI.h>
#include <Wire.h>
#include <components/FlightPin/flight_pin.h>
#include <components/GPS/gps.h>
#include <components/LiPoPower/lipo_power.h>
#include <components/Logger/logger.h>
#include <components/Pressure/pressure.h>
#include <components/Separation/separation.h>

constexpr uint8_t module_id = 'S';
constexpr uint8_t unit_id = 0x41;

// Separation board wiring. Behaviour and protocol live in components.
#define CAN_RX_PIN 44
#define CAN_TX_PIN 43
#define GPS_RX_PIN 38
#define GPS_TX_PIN 39
#define I2C_SDA_PIN 17
#define I2C_SCL_PIN 16
#define SPI0_SCK_PIN 12
#define SPI0_MOSI_PIN 13
#define SPI0_MISO_PIN 11
#define SPI0_CS_PIN 9
#define SD_INSERTED_PIN 10
#define FLIGHT_PIN_PIN 2
#define STATUS_INDICATOR_PIN 42
#define ERROR_INDICATOR_PIN 41

#define ST_PIN 5
#define PG_PIN 4
#define STAT1_PIN 6
#define STAT2_PIN -1
#define HEAT_PIN -1
#define CHARGELED_PIN -1
#define TEMP_PIN -1

constexpr component::Separation::Config separation_config = {
    unit_id,
    14,  // normal LED
    48,  // high-side separation output
    47,  // low-side separation output
    LOW,
};

core::CANBus can_bus(CAN_RX_PIN, CAN_TX_PIN);
core::SerialBus serial_bus(Serial);
component::Logger logger(SPI, SPI0_CS_PIN, SD_INSERTED_PIN);
component::Pressure pressure(Wire, unit_id);
component::FlightPin flight_pin(unit_id, FLIGHT_PIN_PIN);
component::LiPoPower power(Wire, ST_PIN, PG_PIN, STAT1_PIN, STAT2_PIN,
                           HEAT_PIN, CHARGELED_PIN, TEMP_PIN, unit_id);
component::GPS gps(GPS_RX_PIN, GPS_TX_PIN, 115200, unit_id);
component::Separation separation(separation_config);

interface::WatchIndicator<unsigned> status_indicator(STATUS_INDICATOR_PIN,
                                                       kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(ERROR_INDICATOR_PIN,
                                                      kernel::errorCount());

void setup() {
  // This must be first: do not permit either separation output to float while
  // Serial, CAN, or component tasks are starting.
  separation.initializeOutputsSafe();

  Serial.begin(115200);
  delay(1000);

  kernel::setUnitId(unit_id);
  if (!kernel::begin(module_id, true)) return;

  if (!Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN)) return;
  SPI.begin(SPI0_SCK_PIN, SPI0_MISO_PIN, SPI0_MOSI_PIN, SPI0_CS_PIN);

  can_bus.begin();
  serial_bus.begin();

  status_indicator.begin();
  status_indicator.blink_on_change();

  error_indicator.begin();
  error_indicator.set(false);
  error_indicator.blink_on_change(100);

  pressure.begin();
  logger.begin();
  power.begin();
  gps.begin();
  separation.begin();
  flight_pin.begin();
}

void loop() {
  status_indicator.update();
  error_indicator.update();
}
