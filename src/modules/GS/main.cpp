// #define NDEBUG

#include <library/wobc.h>
//#include <components/Logger/logger.h>
//#include <components/LiPoPower/lipo_power_simple.h>

constexpr uint8_t module_id = 0x47;
constexpr uint8_t unit_id = 0x64; // 書き込むユニットごとに変える

// Core
core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);
interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

// Components
//component::Logger logger(SPI);
//component::LiPoPowerSimple power(Wire);

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

  //Wire.setPins();
  // SPI.begin(...)

  status_indicator.begin();
  status_indicator.blink_on_change();
  error_indicator.begin();
  error_indicator.set(true);

  delay(1000);

  can_bus.begin();
  serial_bus.begin();

  //logger.begin();
  //power.begin();
  main_.begin();

  error_indicator.set(false);
  error_indicator.blink_on_change(100);
}

void loop() {
  // put your main code here, to run repeatedly:

  status_indicator.update();
  error_indicator.update();

}


