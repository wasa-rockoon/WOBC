// #define NDEBUG

#include <Arduino.h>
#include <library/wobc.h>

core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);

class Main: public process::Component {
public:
  Main(): process::Component("main", 0x11) {}

  void setup() override {
    // printf("Start");
  }
  void loop() override {
    delay(1000);

    wcpp::Packet p = newPacket(200);

    static unsigned i = 0;
    switch (i % 1) {
    case 0:
      p.command('A', component_id());
      break;
    case 1:
      p.command('B', component_id(), 0x22, 0x33, 12345);
      break;
    case 2:
      p.telemetry('C', component_id());
      break;
    case 3:
      p.telemetry('D', component_id(), 0x22, 0x33, 12345);
      break;
    }
    p.append("Nu").setNull();
    p.append("Ix").setInt(millis());
    p.append("Fy").setFloat32(3.14);
    p.append("By").setString("abcdefghijk");
    auto sub = p.append("St").setStruct();
    sub.append("Sx").setInt(54321);
    sub.append("Sy").setFloat32(3.1415);

    sendPacket(p);

    i++;

    LOG("stack: %d %d", serial_bus.getMaximumStackUsage(), can_bus.getMaximumStackUsage());
  }
} main_;

interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial0.setPins(2, 1);

  if (!kernel::begin(0xFE, false)) return; // module id チェックなし

  status_indicator.begin();
  status_indicator.blink_on_change();
  error_indicator.begin();
  error_indicator.set(true);

  delay(1000);
  // enableCore1WDT();

  can_bus.begin();
  serial_bus.begin();

  main_.begin();

  error_indicator.set(false);
  error_indicator.blink_on_change();
}

void loop() {
  // put your main code here, to run repeatedly:

  status_indicator.update();
  error_indicator.update();

}


