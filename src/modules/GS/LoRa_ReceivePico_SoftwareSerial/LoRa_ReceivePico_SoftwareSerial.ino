#include "SoftwareSerial.h"
#include "E220.h"

/* #define DEBUG */

#define LORA_ADDR E220::BROADCAST
#define LORA_CHANNEL 10

/*#define LORA_TX_PIN 1
#define LORA_RX_PIN 0
#define LORA_AUX_PIN 14
#define LORA_M0_PIN 12
#define LORA_M1_PIN 13*/

#define LORA_TX_PIN 28
#define LORA_RX_PIN 29
#define LORA_AUX_PIN 20
#define LORA_M0_PIN 18
#define LORA_M1_PIN 19

SerialPIO lora_serial(LORA_TX_PIN, LORA_RX_PIN, 256);

E220 lora(lora_serial, LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN);

void setup() {
  Serial.begin(115200);
  lora_serial.begin(9800);

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
  lora_serial.flush();
  lora_serial.end();
  lora_serial.begin(115200);

  delay(1000);

  if (ok) Serial.println("LoRa ok.");
  else Serial.println("LoRa error.");
}

void loop() {
  /*while(lora_serial.available() > 0) {
    Serial.println(" loop");
    Serial.write(lora_serial.read());
  }*/
  //Serial.println(" finish");
  
  uint8_t rx[256];
  unsigned len = lora.receive(rx);
  while (lora.isBusy());

  if (len > 0) {
    Serial.printf("message (%d byte, %d dB): %.*s\n", len, lora.getRSSI(), len, rx);
    /*Serial.printf("message (%d byte, %d dB): %x\n", len, lora.getRSSI(), len, rx);
    for(int i; ) Serial.printf("message (%d byte, %d dB): %x\n", len, lora.getRSSI(), len, rx);*/
  }

  // 送信部分（コメント解除して使用可能）
  /*
  const char message[] = "HELLO LORA!";
  lora.sendTransparent((const uint8_t*)message, sizeof(message) - 1);
  delay(5000);
  */
}
