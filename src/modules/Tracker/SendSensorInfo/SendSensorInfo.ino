#include "Arduino.h"
#include "E220.h"
#include "INA226.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// LoRa設定
#define LORA_ADDR E220::BROADCAST
#define LORA_CHANNEL 10

#define LORA_TX_PIN 38
#define LORA_RX_PIN 39
#define LORA_AUX_PIN 40
#define LORA_M0_PIN 12
#define LORA_M1_PIN 11

#define BME_ADDR 0x76
#define SEALEVELPRESSURE_HPA (1013.25)

INA226 INA1(0x4F);   //Lipoのコネクタから充放電モジュールまで
INA226 INA2(0x4D);   //充放電とバックアップからDCDCまで
INA226 INA3(0x4E);   //DCDC後

// BME280のインスタンス（I2Cモード）
Adafruit_BME280 bme; // I2C

// プロトタイプ宣言
void setBME();
float* getBMEData();
void setupLoRa();
void setupINA();
float* getINAData();

// センサーのデータ取得関数
float BMEData[3]; // 温度、湿度、気圧の順でデータを保持する配列
float INAData[6];
unsigned long lastUpdateTime = 0;

// HardwareSerialのインスタンスを作成
HardwareSerial lora_serial(1); // 通常、1 は Serial1 を指しますが、ボードに応じて変更してください

// E220 LoRaモジュールの初期化
E220 lora(lora_serial, LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN);

void setup() {
  //Serial.begin(115200);
  pinMode(42, OUTPUT);
  pinMode(10, INPUT_PULLUP);
  pinMode(43, INPUT_PULLUP);
  pinMode(44, INPUT_PULLUP);
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);
  setupLoRa();
  setBME();
  setupINA();
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastUpdateTime >= 3000) {
    lastUpdateTime = currentTime;
    digitalWrite(42, HIGH);

    float* data = getBMEData();
    
    // BMEDataの値を文字列に変換
    char message[50];
    snprintf(message, sizeof(message), "BME%.2f,%.2f,%.2f", data[0], data[1], data[2]);
    
    // LoRaでデータを送信
    //lora.sendTransparent((const uint8_t*)message, strlen(message));
    
    float* inadata = getINAData();
    char messageV[50];
    char messageI[50];
    char messageStat[50];
    snprintf(messageV, sizeof(messageV), "INAV %.2f,%.2f,%.2f", inadata[0], inadata[2], inadata[4]);
    snprintf(messageI, sizeof(messageI), "INAI %.2f,%.2f,%.2f", inadata[1], inadata[3], inadata[5]);
    snprintf(messageStat, sizeof(messageStat), "STAT_S1,S2,PG %d,%d,%d", digitalRead(44), digitalRead(43), digitalRead(10));
    
    // LoRaでデータを送信
    delay(1000);
    lora.sendTransparent((const uint8_t*)messageV, strlen(messageV));
    digitalWrite(42, LOW);
    delay(1000);
    lora.sendTransparent((const uint8_t*)messageI, strlen(messageI));
    delay(1000);
    lora.sendTransparent((const uint8_t*)messageStat, strlen(messageStat));
  }
}

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

  /*if (ok) {
    Serial.println("LoRa ok.");
  } else {
    Serial.println("LoRa error.");
  }*/
}

void setBME() {
    Serial.begin(9600);
    delay(100);
    while (!Serial); // シリアル通信の準備ができるまで待機
    //Serial.println(F("BME280 test"));

    unsigned status;
  
    // BME280センサーの初期化
    Wire.begin(17, 16);
    status = bme.begin(BME_ADDR, &Wire);
    delay(200);
    if (!status) {
        /*Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
        Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
        Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
        Serial.print("        ID of 0x60 represents a BME 280.\n");
        Serial.print("        ID of 0x61 represents a BME 680.\n");*/
        while (1) delay(10);
    }
    
    //Serial.println("-- Default Test --");
    delay(1000);
}

float* getBMEData() {
    BMEData[0] = bme.readTemperature();
    BMEData[1] = bme.readHumidity();
    BMEData[2] = bme.readPressure() / 100.0F;

    return BMEData;
}

void setupINA(){
  Wire.begin(17, 16);
  if (!INA1.begin() )
  {
    //Serial.println("could not connect. Fix and Reboot");
    while(1);
  }
  INA1.setMaxCurrentShunt(1, 0.05);
  if (!INA2.begin() )
  {
    //Serial.println("could not connect. Fix and Reboot");
    while(1);
  }
  INA2.setMaxCurrentShunt(1, 0.05);
  if (!INA3.begin() )
  {
    //Serial.println("could not connect. Fix and Reboot");
    while(1);
  }
  INA3.setMaxCurrentShunt(1, 0.05);
}

float* getINAData() {
    INAData[0] = INA1.getBusVoltage();
    INAData[1] = INA1.getCurrent_mA();
    INAData[2] = INA2.getBusVoltage();
    INAData[3] = INA2.getCurrent_mA();
    INAData[4] = INA3.getBusVoltage();
    INAData[5] = INA3.getCurrent_mA();
    return INAData;
}
