#include <Arduino.h>
// #include <library/wobc.h>


// class CoreTest: public process::Module {
// public:
//   CoreTest()
//   : Module("CoreTest", 0xFE),
//     serial_bus_(Serial) {}

//   void setup() override {
//     // start(serial_bus_);
//   }

//   void loop() override {
//     delay(1000);
//     Serial.print("A\n");
//   }

// private:
//   core::SerialBus serial_bus_;

// } module;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  // module.main();
}

void loop() {
  // put your main code here, to run repeatedly:

  Serial.print(".");
  delay(1000);
}


