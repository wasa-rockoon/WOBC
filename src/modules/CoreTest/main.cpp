#include <Arduino.h>
#include <library/wobc.h>


class Main: public process::Component {
public:
  Main(): process::Component("main", 0) {}

  void setup() override {
    Serial.print("START");
  }
  void loop() override {
    delay(1000);

    wcpp::Packet p = newPacket(200);

    static unsigned i = 0;
    switch (i % 4) {
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

core::SerialBus serial_bus(Serial);
Main main_;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  serial_bus.begin();
  main_.begin();
}

void loop() {
  // put your main code here, to run repeatedly:

  vTaskDelay(1000);

}


