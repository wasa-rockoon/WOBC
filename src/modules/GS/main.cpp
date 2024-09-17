// #define NDEBUG

#include <library/wobc.h>
#include <components/Telemeter/telemeter.h>
#include <components/Logger/logger.h>
//#include <components/LiPoPower/lipo_power_simple.h>
#include <SPI.h>

#define SPI0_SCK_PIN 12
#define SPI0_MOSI_PIN 11
#define SPI0_MISO_PIN 13
#define SPI0_CS_PIN 10

#define SD_INSERTED_PIN 9
#define SDCARD_MOSI_PIN SPI0_MOSI_PIN
#define SDCARD_MISO_PIN SPI0_MISO_PIN
#define SDCARD_SS_PIN SPI0_CS_PIN
#define SDCARD_SCK_PIN SPI0_SCK_PIN

constexpr uint8_t module_id = 0x47;
constexpr uint8_t unit_id = 0x64; // 書き込むユニットごとに変える

// Core
core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);
interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

// Components
component::Logger logger(SPI, SPI0_CS_PIN, SD_INSERTED_PIN);
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
    /*uint8_t buf[255];
    memset(buf, 0, 255);

    wcpp::Packet tracker_packet = wcpp::Packet::empty(buf, 255);
    tracker_packet.telemetry('A', 0x11, 'a', 0x11, 12345);
    tracker_packet.append("La").setFloat64(35.7087377); 
    tracker_packet.append("Lo").setFloat64(139.7170736);
    tracker_packet.append("Al").setInt(1234);

    // Time
    tracker_packet.append("Ut").setInt(1234);
    tracker_packet.append("Ts").setInt(1234);

    // Power
    tracker_packet.append("Vb").setInt(1111);
    tracker_packet.append("Vp").setInt(1112);
    tracker_packet.append("Vd").setInt(1113);
    tracker_packet.append("Ip").setInt(2111);
    tracker_packet.append("Id").setInt(2112);

    // Environment
    tracker_packet.append("Pr").setInt(1013);
    tracker_packet.append("Te").setInt(29);
    tracker_packet.append("Hu").setInt(78);
    tracker_packet.append("Pa").setInt(100000);
    
    sendPacket(tracker_packet);*/
  }
} main_;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial0.setPins(2, 1);

  kernel::setUnitId(unit_id); // unit id を設定（mainモジュールのみ）
  if (!kernel::begin(module_id, true)) return; // check module id

  //Wire.setPins();
  SPI.begin(SDCARD_SCK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN, SDCARD_SS_PIN);

  status_indicator.begin();
  status_indicator.blink_on_change();
  error_indicator.begin();
  error_indicator.set(true);

  delay(1000);

  can_bus.begin();
  serial_bus.begin();

  logger.begin();
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


