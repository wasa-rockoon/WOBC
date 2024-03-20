
#include "SoftwareSerial.h"
#include "E220.h"

/* #define DEBUG */

#define LORA_ADDR E220::BROADCAST
#define LORA_CHANNEL 10

#define LORA_TX_PIN 0
#define LORA_RX_PIN 1
#define LORA_AUX_PIN 22
#define LORA_M0_PIN 21
#define LORA_M1_PIN 20

//SoftwareSerial lora_serial(LORA_RX_PIN, LORA_TX_PIN);


E220 lora(Serial1, LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN);

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);

  Serial1.begin(9600);
  lora.begin();
  bool ok = true;
  ok &= lora.setMode(E220::Mode::CONFIG_DS);
  ok &= lora.setParametersToDefault();
  ok &= lora.setSerialBaudRate(115200);
  ok &= lora.setDataRate(E220::SF::SF9, E220::BW::BW125kHz);
  ok &= lora.setEnvRSSIEnable(true);
  ok &= lora.setSendMode(E220::SendMode::TRANSPARENT);
  ok &= lora.setModuleAddr(LORA_ADDR);
  ok &= lora.setChannel(LORA_CHANNEL);
  ok &= lora.setRSSIEnable(true);
  ok &= lora.setMode(E220::Mode::NORMAL);
  Serial1.flush();
  Serial1.begin(115200);

  delay(1000);
  if (ok) Serial.println("LoRa ok.");
  else Serial.println("LoRa error.");
}


void loop() {
  uint8_t rx[256];
  unsigned len = lora.receive(rx);
  while (lora.isBusy());

  if (len > 0) {
    Serial.printf("message (%d byte, %d dB): %.*s\n", len, lora.getRSSI(), len, rx);
  }
  
  const char message[] = "HELLO LORA!";
  //Serial.println("SENT");
  lora.sendTransparent((const uint8_t*)message, 12);
  delay(5000);
}

/*void loop1() {
  delay(5000);
  while (lora.isBusy());
  const char message[] = "HELLO LORA!";
  Serial.println("SENT");
  lora.sendTransparent((const uint8_t*)message, 12);
}*/
