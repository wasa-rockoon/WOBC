// #define NDEBUG

#include <library/wobc.h>
#include <components/Logger/logger.h>
#include <components/LoRa/lora.h>
#include <components/LiPoPower/lipo_power_simple.h>
#include <components/Pressure/pressure.h> // テスト用

constexpr uint8_t module_id = 0xF0; // TBD
constexpr uint8_t unit_id = 0x01; // TBD

// Core
core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);
interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

// Components
component::Logger logger(SPI);
component::LoRa lora(Serial1, 0, 0, 0); // TBD
component::LiPoPowerSimple power(Wire);
component::Pressure pressure(Wire);


class Main: public process::Component {
public:
  Main(): process::Component("main", 0x00) {}

  void setup() override {

  }

  void loop() override {
    delay(1000);
    LOG("GS working");
  }
} main_;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial0.setPins(2, 1);

  kernel::setUnitId(unit_id); // unit id を設定（mainモジュールのみ）
  if (!kernel::begin(module_id, true)) return; // check module id

  // Wire.setPins();
  // SPI.begin(...)

  status_indicator.begin();
  status_indicator.blink_on_change();
  error_indicator.begin();
  error_indicator.set(true);

  can_bus.begin();
  serial_bus.begin();

  delay(1000);

  logger.begin();
  lora.begin();
  power.begin();
  pressure.begin();
  main_.begin();

  error_indicator.set(false);
  error_indicator.blink_on_change(100);
}

void loop() {
  // put your main code here, to run repeatedly:

  status_indicator.update();
  error_indicator.update();

}


