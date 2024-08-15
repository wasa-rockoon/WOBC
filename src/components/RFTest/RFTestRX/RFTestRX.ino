#include "SoftwareSerial.h"
#include "E220.h"

/* #define DEBUG */

#define LORA_ADDR E220::BROADCAST
#define LORA1_CHANNEL 10
#define LORA2_CHANNEL 4

#define LORA1_TX_PIN 1
#define LORA1_RX_PIN 0
#define LORA1_AUX_PIN 14
#define LORA1_M0_PIN 12
#define LORA1_M1_PIN 13
#define LORA1_SW1 10
#define LORA1_SW2 11

#define LORA2_TX_PIN 28
#define LORA2_RX_PIN 29
#define LORA2_AUX_PIN 20
#define LORA2_M0_PIN 18
#define LORA2_M1_PIN 19
#define LORA2_SW1 26
#define LORA2_SW2 27

#define STAT 25
#define ERROR 24

SerialPIO lora1_serial(LORA1_TX_PIN, LORA1_RX_PIN, 256);
SerialPIO lora2_serial(LORA2_TX_PIN, LORA2_RX_PIN, 256);

E220 lora1(lora1_serial, LORA1_AUX_PIN, LORA1_M0_PIN, LORA1_M1_PIN);
E220 lora2(lora2_serial, LORA2_AUX_PIN, LORA2_M0_PIN, LORA2_M1_PIN);

void setup() {
  Serial.begin(115200);
  pinMode(STAT, OUTPUT);
  pinMode(ERROR, OUTPUT);

  pinMode(LORA1_SW1, OUTPUT);
  pinMode(LORA1_SW2, OUTPUT);
  pinMode(LORA2_SW1, OUTPUT);
  pinMode(LORA2_SW2, OUTPUT);

  digitalWrite(LORA1_SW1,HIGH);
  digitalWrite(LORA1_SW2,LOW);
  digitalWrite(LORA2_SW1,HIGH);
  digitalWrite(LORA2_SW2,LOW);

  delay(500);
  lora1_serial.begin(9800);
  lora1.begin();
  bool ok1 = true;
  ok1 &= lora1.setMode(E220::Mode::CONFIG_DS);
  ok1 &= lora1.setParametersToDefault();
  ok1 &= lora1.setSerialBaudRate(115200);
  ok1 &= lora1.setDataRate(E220::SF::SF9, E220::BW::BW125kHz);
  ok1 &= lora1.setEnvRSSIEnable(true);
  ok1 &= lora1.setSendMode(E220::SendMode::TRANSPARENT);
  ok1 &= lora1.setModuleAddr(LORA_ADDR);
  ok1 &= lora1.setChannel(LORA1_CHANNEL);
  ok1 &= lora1.setRSSIEnable(true);
  ok1 &= lora1.setMode(E220::Mode::NORMAL);
  lora1_serial.flush();
  lora1_serial.end();
  lora1_serial.begin(115200);
  delay(500);
  if (ok1) Serial.println("LoRa1 ok.");
  else Serial.println("LoRa1 error.");

  lora2_serial.begin(9800);
  lora2.begin();
  bool ok2 = true;
  ok2 &= lora2.setMode(E220::Mode::CONFIG_DS);
  ok2 &= lora2.setParametersToDefault();
  ok2 &= lora2.setSerialBaudRate(115200);
  ok2 &= lora2.setDataRate(E220::SF::SF9, E220::BW::BW125kHz);
  ok2 &= lora2.setEnvRSSIEnable(true);
  ok2 &= lora2.setSendMode(E220::SendMode::TRANSPARENT);
  ok2 &= lora2.setModuleAddr(LORA_ADDR);
  ok2 &= lora2.setChannel(LORA2_CHANNEL);
  ok2 &= lora2.setRSSIEnable(true);
  ok2 &= lora2.setMode(E220::Mode::NORMAL);
  lora2_serial.flush();
  lora2_serial.end();
  lora2_serial.begin(115200);
  delay(500);
  if (ok2) Serial.println("LoRa2 ok.");
  else Serial.println("LoRa2 error.");
}

void loop() {
  uint8_t rx1[256];
  unsigned len1 = lora1.receive(rx1);
  digitalWrite(STAT, LOW);
  if (len1 > 0) {
    digitalWrite(STAT, HIGH);
    Serial.printf("message1 (%d byte, %d dB):\n", len1, lora1.getRSSI());
    
    Serial.print("Hex: ");
    for (unsigned i = 0; i < len1; i++) {
      Serial.printf("%02X", rx1[i]);  // 16進数で表示
    }
    Serial.println();

    /*Serial.print("ASCII: ");
    for (unsigned i = 0; i < len1; i++) {
      if (rx1[i] >= 32 && rx1[i] <= 126) { // 表示可能なASCII文字のみ表示
        Serial.write(rx1[i]);
      } else {
        Serial.print('.');
      }
    }*/
    Serial.println();
    digitalWrite(STAT,LOW);
  }

  uint8_t rx2[256];
  unsigned len2 = lora2.receive(rx2);
  digitalWrite(ERROR, LOW);
  if (len2 > 0) {
    digitalWrite(ERROR, HIGH);
    Serial.printf("message2 (%d byte, %d dB):\n", len2, lora2.getRSSI());
    
    Serial.print("Hex: ");
    for (unsigned i = 0; i < len2; i++) {
      Serial.printf("%02X", rx2[i]);  // 16進数で表示
    }
    Serial.println();

    /*Serial.print("ASCII: ");
    for (unsigned i = 0; i < len2; i++) {
      if (rx2[i] >= 32 && rx2[i] <= 126) { // 表示可能なASCII文字のみ表示
        Serial.write(rx2[i]);
      } else {
        Serial.print('.');
      }
    }*/
    Serial.println();
    Serial.println("---------------------------------");
    digitalWrite(ERROR, LOW);
  }
}