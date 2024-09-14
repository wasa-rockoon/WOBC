// #define NDEBUG

#include <library/wobc.h>
#include <components/Telemeter/telemeter.h>
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
component::Telemeter telemeter;

class Main: public process::Component {
public:
  Main(): process::Component("main", 0x00) {}

  void setup() override {

  }

  void loop() override {
    delay(1000);
    LOG("GS working");
    wcpp::Packet p = newPacket(64);
    p.telemetry('A', 0x11, 0x22, 0x33, 12345);
    p.append("La").setInt(1351234);
    p.append("Lo").setInt(351234);
    p.append("Al").setInt(1234);
    p.append("Ti").setInt(1234);
    p.append("Va").setInt(1111);
    p.append("Vb").setInt(1112);
    p.append("Vc").setInt(1113);
    p.append("Pr").setFloat32(1013.12);
    p.append("Te").setInt(29);
    p.append("Hu").setInt(78);
    p.append("Pa").setFloat32(1013.12);
    sendPacket(p);
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
  telemeter.begin();
  main_.begin();

  error_indicator.set(false);
  error_indicator.blink_on_change(100);
}

void loop() {
  // put your main code here, to run repeatedly:

  status_indicator.update();
  error_indicator.update();

}


