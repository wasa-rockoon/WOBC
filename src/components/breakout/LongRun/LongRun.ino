#include <TinyGPSPlus.h>
#include <Arduino.h>
#include <Wire.h>
#include "E220.h"
#include "INA226.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>

Preferences preferences;

#define ERROR 41
#define STAT 42

#define LORA_ADDR E220::BROADCAST
#define LORA_CHANNEL 10
#define LORA_TX_PIN 38
#define LORA_RX_PIN 39
#define LORA_AUX_PIN 40
#define LORA_M0_PIN 12
#define LORA_M1_PIN 11

#define BME_ADDR 0x76
#define SEALEVELPRESSURE_HPA (1013.25)

#define SPI0_SCK_PIN 1
#define SPI0_MOSI_PIN 4
#define SPI0_MISO_PIN 3
#define SPI0_CS_PIN 2

#define SD_INSERTED_PIN 5
#define SDCARD_MOSI_PIN SPI0_MOSI_PIN
#define SDCARD_MISO_PIN SPI0_MISO_PIN
#define SDCARD_SS_PIN SPI0_CS_PIN
#define SDCARD_SCK_PIN SPI0_SCK_PIN

#define SD_TIMEOUT_MS 10

INA226 INA1(0x4F);   // Lipoのコネクタから充放電モジュールまで
INA226 INA2(0x4D);   // 充放電とバックアップからDCDCまで
INA226 INA3(0x4E);   // DCDC後

Adafruit_BME280 bme;

void setBME();
float* getBMEData();
void setupINA();
float* getINAData();
void setupLoRa();

float INAData[6];
float BMEData[3]; // 温度、湿度、気圧の順でデータを保持する配列

char fileName[13];
int fileNumber;

static const int RXPin = 47, TXPin = 48;
static const uint32_t GPSBaud = 38400;

TinyGPSPlus gps;
HardwareSerial ss(2);

HardwareSerial lora_serial(1);
E220 lora(lora_serial, LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN);

unsigned long lastUpdateTime = 0;

void setup() {
  Serial.begin(115200);
  digitalWrite(STAT, HIGH);
  Wire.begin(17, 16);
  ss.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin);
  pinMode(STAT, OUTPUT);
  pinMode(ERROR, OUTPUT);
  SPI.begin(SDCARD_SCK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN, SDCARD_SS_PIN);
  
  setupLoRa();
  setupINA();
  setBME();

  delay(500);
  if (!SD.begin(SDCARD_SS_PIN)) { 
    Serial.println(F("Card failed, or not present")); 
    digitalWrite(ERROR, HIGH);
    return;
  }
  Serial.println(F("SD ok."));
}

void loop() {
  char gpsData[64];
  char VI[50];
  char TH[50];

  preferences.begin("my-app", false);
  fileNumber = preferences.getInt("value", 0);
  snprintf(fileName, sizeof(fileName), "/file%d.txt", fileNumber);
  File dataFile = SD.open(fileName, FILE_WRITE);

  if (dataFile) {
    dataFile.println("V1,I1,V2,I2,V3,I3,Temp,Hum,Press,lat,lng,month,day,hour,min,sec");
  } else {
    Serial.println("Failed to open file for writing");
    digitalWrite(ERROR, HIGH);
    return;
  }
  fileNumber++;
  preferences.putInt("value", fileNumber);
  preferences.end();

  for (int i = 0; i < 600; i++) {
    digitalWrite(STAT, HIGH);
    float* inadata = getINAData();
    snprintf(VI, sizeof(VI), "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,", inadata[0], inadata[1], inadata[2], inadata[3], inadata[4], inadata[5]);
    dataFile.print(VI);

    float* bmedata = getBMEData();
    snprintf(TH, sizeof(TH), "%.2f,%.2f,%.2f,", bmedata[0], bmedata[1], bmedata[2]);
    dataFile.print(TH);

    unsigned long currentTime = millis();

    if (currentTime - lastUpdateTime >= 3000) {
      lastUpdateTime = currentTime;

      lora.sendTransparent((const uint8_t*)VI, strlen(VI));
      delay(1000);
      lora.sendTransparent((const uint8_t*)TH, strlen(TH));
      delay(1000);
    }

    while (ss.available() > 0) {
      gps.encode(ss.read());
    }

    if (gps.charsProcessed() < 10 && millis() > 5000) {
      Serial.println(F("No GPS data received: check wiring"));
    }

    if (gps.location.isUpdated()) {
      float lat = gps.location.lat();
      float lng = gps.location.lng();
      int month = gps.date.month();
      int day = gps.date.day();
      int hour = (gps.time.hour() + 9) % 24;
      int min = gps.time.minute();
      int sec = gps.time.second();
      sprintf(gpsData, "%.6f,%.6f,%02d,%02d,%02d,%02d,%02d", lat, lng, month, day, hour, min, sec);
      dataFile.print(gpsData);
      digitalWrite(STAT, LOW);
      lora.sendTransparent((const uint8_t*)gpsData, strlen(gpsData));
      delay(1000);
    }
    dataFile.println();
    i++;
    dataFile.flush();
    delay(2000);
  }
  dataFile.close();
}

void setupLoRa() {
  lora_serial.begin(9600, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);

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
  lora_serial.begin(115200);

  delay(100);

  if (ok) {
    Serial.println("LoRa setup complete.");
  } else {
    Serial.println("LoRa setup error.");
  }
}

void setupINA() {
  INA1.begin();
  INA1.setMaxCurrentShunt(1, 0.05);
  INA2.begin();
  INA2.setMaxCurrentShunt(1, 0.05);
  INA3.begin();
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

void setBME() {
  unsigned status;
  status = bme.begin(BME_ADDR, &Wire);
  delay(200);
  if (!status) {
    while (1) delay(10);
  }
  delay(1000);
}

float* getBMEData() {
  BMEData[0] = bme.readTemperature();
  BMEData[1] = bme.readHumidity();
  BMEData[2] = bme.readPressure() / 100.0F;
  return BMEData;
}
