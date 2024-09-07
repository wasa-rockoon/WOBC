// #define NDEBUG

#include <library/wobc.h>
#include <components/LiPoPower/lipo_power.h>

#define ST 6
#define PG 10
#define STAT1 43
#define STAT2 44
#define HEAT 9
#define CHARGELED 8
#define TEMP 7

core::SerialBus serial_bus(Serial);
component::LiPoPower power(Wire, ST, PG, STAT1, STAT2, HEAT, CHARGELED, TEMP, 1);

constexpr uint8_t module_id = 0x54;
constexpr uint8_t unit_id = 0x61;

interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

class Main: public process::Component {
public:
  Main(): process::Component("main", 0x00) {}
  Listener all_packets_;

  void setup() override {
    listen(all_packets_, 8);
  }

  void loop() override {
    while (all_packets_) {
    const wcpp::Packet& packet = all_packets_.pop();
    Serial.print(packet);
  }
    delay(1000);
  }
} main_;

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);

  kernel::setUnitId(unit_id); // unit id を設定（mainモジュールのみ）
  if (!kernel::begin(module_id, true)) return; // check module id

  Serial0.setPins(2, 1);
  Wire.begin(17, 16);

  delay(1000);
  status_indicator.begin();
  status_indicator.blink_on_change();
  error_indicator.begin();
  error_indicator.set(true);

  power.begin();
  main_.begin();

  error_indicator.set(false);
  error_indicator.blink_on_change(100);
}

void loop() {
  // put your main code here, to run repeatedly:

  status_indicator.update();
  error_indicator.update();
}


