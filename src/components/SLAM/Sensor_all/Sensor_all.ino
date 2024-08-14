#include "BMI088.h"
#include "Wire.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <QMC5883LCompass.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

#define ADDRESS 0x76

#define SEALEVELPRESSURE_HPA (1013.25)

static const int RXPin = 0, TXPin = 1;
static const uint32_t GPSBaud = 9600;

/* accel object */
Bmi088Accel accel(Wire1,0x19);
/* gyro object */
Bmi088Gyro gyro(Wire1,0x69);

Adafruit_BME280 bme; // I2C

QMC5883LCompass compass;

// The TinyGPSPlus object
TinyGPSPlus gps;
// The serial connection to the GPS device
SoftwareSerial ss(RXPin, TXPin);

unsigned long delayTime;

void setup() 
{
  int status;
  /* USB Serial to print data */
  Serial.begin(115200);
  delay(100);
  while(!Serial) {}
  Serial.println(F("BMI088 BME280 QMC5883L test"));
  /* start the sensors */
  Wire1.setSDA(2);
  Wire1.setSCL(3);
  Wire1.begin();
  status = accel.begin();
  if (status < 0) {
    Serial.println("Accel Initialization Error");
    Serial.println(status);
    while (1) {}
  }
  status = gyro.begin();
  if (status < 0) {
    Serial.println("Gyro Initialization Error");
    Serial.println(status);
    while (1) {}
  }
  //status = bme.begin(ADDRESS, &Wire);  
  // You can also pass in a Wire library object like &Wire2
  Wire.setSDA(4);
  Wire.setSCL(5);
  status = bme.begin(0x76, &Wire);
  if (status < 0) {
    Serial.println("bme Initialization Error");
    Serial.println(status);
    while (1) {}
  }
  Wire.setSDA(4);
  Wire.setSCL(5);
  compass.init();
  ss.begin(GPSBaud);

  delayTime = 20;

  Serial.println();
}

void loop() 
{  
  /* read the accel */
  accel.readSensor();
  /* read the gyro */
  gyro.readSensor();
  // Read compass values
  compass.read();

  /* print the data */
  Serial.print(accel.getAccelX_mss());
  Serial.print("\t");
  Serial.print(accel.getAccelY_mss());
  Serial.print("\t");
  Serial.print(accel.getAccelZ_mss());
  Serial.print("\t");
  Serial.print(gyro.getGyroX_rads());
  Serial.print("\t");
  Serial.print(gyro.getGyroY_rads());
  Serial.print("\t");
  Serial.print(gyro.getGyroZ_rads());
  Serial.print("\t");
  Serial.print(compass.getX());
  Serial.print("\t");
  Serial.print(compass.getY());
  Serial.print("\t");
  Serial.print(compass.getZ());
  Serial.print("\t");
  Serial.print(bme.readTemperature());
  Serial.print("\t");
  Serial.print(bme.readPressure() / 100.0F);
  Serial.print("\t");
  while (ss.available()){
    gps.encode(ss.read());
  }
  if (millis() > 5000 && gps.charsProcessed() < 10) {
    Serial.println(F("No GPS data received: check wiring"));
  }
  Serial.print("\t");
  printFloat(gps.location.lat(), gps.location.isValid(), 11, 6);
  printFloat(gps.location.lng(), gps.location.isValid(), 12, 6);
  printDateTime(gps.date, gps.time);
  printFloat(gps.altitude.meters(), gps.altitude.isValid(), 7, 2);
  printFloat(gps.speed.kmph(), gps.speed.isValid(), 6, 2);
  printInt(gps.charsProcessed(), true, 6);
  printInt(gps.failedChecksum(), true, 9);


  Serial.println();
  
  /* delay to help with printing */
  delay(delayTime);
}

static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do 
  {
    while (ss.available())
      gps.encode(ss.read());
  } while (millis() - start < ms);
}

static void printFloat(float val, bool valid, int len, int prec)
{
  if (!valid)
  {
    while (len-- > 1)
      Serial.print('*');
    Serial.print(' ');
  }
  else
  {
    Serial.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1); // . and -
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i)
      Serial.print(' ');
  }
  smartDelay(0);
}

static void printInt(unsigned long val, bool valid, int len)
{
  char sz[32] = "*****************";
  if (valid)
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  Serial.print(sz);
  smartDelay(0);
}

static void printDateTime(TinyGPSDate &d, TinyGPSTime &t)
{
  if (!d.isValid())
  {
    Serial.print(F("********** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d ", d.month(), d.day(), d.year());
    Serial.print(sz);
  }
  
  if (!t.isValid())
  {
    Serial.print(F("******** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d:%02d:%02d ", (t.hour() + 9) % 24, t.minute(), t.second());
    Serial.print(sz);
  }

  printInt(d.age(), d.isValid(), 5);
  smartDelay(0);
}