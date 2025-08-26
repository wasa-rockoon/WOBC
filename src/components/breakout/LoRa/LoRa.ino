#include "Arduino.h"
#include "E220.h"

// LoRa設定
#define LORA_ADDR E220::BROADCAST
#define LORA_CHANNEL 10

#define LORA_TX_PIN 38
#define LORA_RX_PIN 39
#define LORA_AUX_PIN 40
#define LORA_M0_PIN 12
#define LORA_M1_PIN 11

// HardwareSerialのインスタンスを作成
HardwareSerial lora_serial(1); // 通常、1 は Serial1 を指しますが、ボードに応じて変更してください

// E220 LoRaモジュールの初期化
E220 lora(lora_serial, LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN);


void setup() {
  Serial.begin(115200);
  lora_serial.begin(9600, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN); // lora_serialの初期化

  // LoRaモジュールの初期化と設定
  lora.begin();
  bool ok = true;
  ok &= lora.setMode(E220::Mode::CONFIG_DS);
  ok &= lora.setParametersToDefault();
  ok &= lora.setSerialBaudRate(115200); // 初期シリアルボーレートに戻す
  ok &= lora.setDataRate(E220::SF::SF9, E220::BW::BW125kHz);
  ok &= lora.setEnvRSSIEnable(true);
  ok &= lora.setSendMode(E220::SendMode::TRANSPARENT);
  ok &= lora.setModuleAddr(LORA_ADDR);
  ok &= lora.setChannel(LORA_CHANNEL);
  ok &= lora.setRSSIEnable(true);
  ok &= lora.setMode(E220::Mode::NORMAL);
  lora_serial.flush();
  lora_serial.begin(115200);

  delay(1000);

  if (ok) {
    Serial.println("LoRa ok.");
  } else {
    Serial.println("LoRa error.");
  }
  
}

void loop() {
  uint8_t rx[256];
  
  unsigned len = lora.receive(rx);
  while (lora.isBusy());

  if (len > 0) {
    Serial.printf("message (%d byte, %d dB): %.*s\n", len, lora.getRSSI(), len, rx);
  }
  
  const char message[] = "HELLO LORA!";
  lora.sendTransparent((const uint8_t*)message, sizeof(message) - 1);
  
  delay(5000);
}

/*void loop1() {
  delay(5000);
  while (lora.isBusy());
  const char message[] = "HELLO LORA!";
  Serial.println("SENT");
  lora.sendTransparent((const uint8_t*)message, 12);
}*/
