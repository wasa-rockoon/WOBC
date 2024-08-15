#include "Arduino.h"
//#include "SoftwareSerial.h"
#include "E220.h"

/* #define DEBUG */

#define LORA_ADDR E220::BROADCAST
#define LORA_CHANNEL 4

#define LORA_TX_PIN 38
#define LORA_RX_PIN 39
#define LORA_AUX_PIN 40
#define LORA_M0_PIN 12
#define LORA_M1_PIN 11

#define STAT 42

void setupLoRa();
HardwareSerial lora_serial(1);
//SoftwareSerial lora_serial(LORA_RX_PIN, LORA_TX_PIN);

E220 lora(lora_serial, LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(STAT, OUTPUT);
  setupLoRa();
}

void loop() {
  uint8_t rx[256];
  unsigned len = lora.receive(rx);
  while (lora.isBusy());

  if (len > 0) {
    Serial.printf("message (%d byte, %d dB): %.*s\n", len, lora.getRSSI(), len, rx);
  }
  
  // 200バイトのメッセージを作成
  uint8_t message[100];
  for (int i = 0; i < 100; i++) {
    message[i] = i % 256; // 0〜255の範囲で値を設定
  }

  // 送信するメッセージをシリアルモニタに表示
  Serial.print("Sending message: ");
  for (int i = 0; i < 100; i++) {
    Serial.printf("%02X", message[i]); // 16進数で表示
  }
  Serial.println();
  
  // メッセージを送信
  digitalWrite(STAT, HIGH);
  lora.sendTransparent(message, 100);
  delay(1000);
  digitalWrite(STAT, LOW);
  delay(2000);
}

/*void loop1() {
  delay(5000);
  while (lora.isBusy());
  const char message[] = "HELLO LORA!";
  Serial.println("SENT");
  lora.sendTransparent((const uint8_t*)message, 12);
}*/

void setupLoRa() {
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
    digitalWrite(STAT, HIGH);
  } else {
    Serial.println("LoRa error.");
  }
}
