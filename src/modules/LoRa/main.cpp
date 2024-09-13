// #define NDEBUG

#include <Arduino.h>
#include <library/wobc.h>
#include <components/LoRa/lora.h>

// Pins
#define LORA_CHANNEL 10
#define LORA_TX_PIN 1
#define LORA_RX_PIN 0
#define LORA_AUX_PIN 14
#define LORA_M0_PIN 12
#define LORA_M1_PIN 13
#define LORA_SW_A1 10
#define LORA_SW_A2 11

// Settings
constexpr uint8_t module_id = 0x4C;

// IO
SerialPIO serial1(LORA_TX_PIN, LORA_RX_PIN, 256);
driver::GenericSerial<SerialPIO> lora_serial1(serial1);

// Core
core::CANBus can_bus(23, 22);
core::SerialBus serial_bus(Serial);
interface::WatchIndicator<unsigned> status_indicator(25, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(24, kernel::errorCount());

// Components

component::LoRa lora(lora_serial1, LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN, LORA_SW_A1, LORA_SW_A2, LORA_CHANNEL);

class Main: public process::Component {
public:
  Main(): process::Component("main", 0x11) {}

  void setup() override {
  }
  void loop() override {
    delay(1000);
    LOG("LoRa working");
  }
} main_;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  if (!kernel::begin(module_id, false)) return;

  status_indicator.begin();
  status_indicator.blink_on_change();
  error_indicator.begin();
  error_indicator.set(true);

  delay(1000);

  lora.begin();
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

  // log_d("[%d %d]", serial_bus.getMaximumStackUsage(), status_indicator.getMaximumStackUsage());
  // main_.LOG("stack: %d %d %d", F_CPU, serial_bus.getMaximumStackUsage(), can_bus.getMaximumStackUsage());
}


