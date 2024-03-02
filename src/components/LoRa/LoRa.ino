
#include "SoftwareSerial.h"
#include "E220.h"

/* #define DEBUG */

#define LORA_ADDR E220::BROADCAST
#define LORA_CHANNEL 17

#define LORA_TX_PIN 0
#define LORA_RX_PIN 0
#define LORA_AUX_PIN 0
#define LORA_M0_PIN 0
#define LORA_M1_PIN 0

SoftwareSerial lora_serial(LORA_RX_PIN, LORA_TX_PIN);

E220 lora(lora_serial, LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN);

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);

  lora_serial.begin(9600);
  lora.begin();
  bool ok = true;
  ok &= lora.setMode(E220::Mode::CONFIG_DS);
  ok &= lora.setParametersToDefault();
  ok &= lora.setSerialBaudRate(9600);
  ok &= lora.setDataRate(E220::SF::SF9, E220::BW::BW125kHz);
  ok &= lora.setEnvRSSIEnable(true);
  ok &= lora.setSendMode(E220::SendMode::TRANSPARENT);
  ok &= lora.setModuleAddr(LORA_ADDR);
  ok &= lora.setChannel(LORA_CHANNEL);
  ok &= lora.setRSSIEnable(true);
  ok &= lora.setMode(E220::Mode::NORMAL);

  if (ok) Serial.println("LoRa ok.");
  else Serial.println("LoRa error.");
}


void loop() {
  uint8_t rx[256];
  unsigned len = lora.receive(rx);

  if (len > 0) {
    Serial.printf("message (%d byte, %d dB): %.*s\n", len, lora.getRSSI(), len, rx);
  }
}

void loop1() {
  delay(5000);
  while (!lora.isBusy());
  const char message[] = "HELLO LORA!";
  lora.sendTransparent((const uint8_t*)message, 12);
}
