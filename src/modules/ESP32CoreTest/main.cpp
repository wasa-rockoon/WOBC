// #define NDEBUG

#include <Arduino.h>
#include <library/wobc.h>

class Main: public process::Component {
public:
  Main(): process::Component("main", 0) {}

  void setup() override {
    // printf("Start");
  }
  void loop() override {
    delay(1000);

    wcpp::Packet p = newPacket(200);

    static unsigned i = 0;
    switch (i % 1) {
    case 0:
      p.command('A', 0x11);
      break;
    case 1:
      p.command('B', 0x11, 0x22, 0x33, 12345);
      break;
    case 2:
      p.telemetry('C', 0x11);
      break;
    case 3:
      p.telemetry('D', 0x11, 0x22, 0x33, 12345);
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
  }
};

core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);
core::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
// core::WatchIndicator<unsigned> error_indicator(41, kernel::anomalyCount());
Main main_;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial0.setPins(2, 1);

  delay(1000);
  // enableCore1WDT();

  can_bus.begin();
  serial_bus.begin();
  status_indicator.begin();
  // error_indicator.begin();
  main_.begin();

  pinMode(41, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:

  digitalWrite(41, LOW);
  vTaskDelay(1000);
  digitalWrite(41, HIGH);
  vTaskDelay(1000);
  // log_d("[%d %d]", serial_bus.getMaximumStackUsage(), status_indicator.getMaximumStackUsage());
  // main_.LOG("stack: %d %d", serial_bus.getMaximumStackUsage(), can_bus.getMaximumStackUsage());
}


