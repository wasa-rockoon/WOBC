#include <Arduino.h>
#include "INA226.h"

#define BLOCK_MAX 10

int ver = 0;
int State = LOW;
int ign1_pin = 26;
int ign2_pin = 27;
int alart_pin = 28;
unsigned long OnTime = 8000;
unsigned long previousMillis = 0;
unsigned long interval = 500;

INA226 INA(0x40);

class igniter {
  int ignaiter1_pin;
  int ignaiter2_pin;
  int duty;
  unsigned long ontime;
  float I0 = 1000;
  float k = 0.05;

  public:
  igniter(int pin1, int pin2, unsigned long time) {
    ignaiter1_pin = pin1;
    ignaiter2_pin = pin2;
    pinMode(ignaiter1_pin, OUTPUT);
    pinMode(ignaiter2_pin, OUTPUT);

    ontime = time;
  }

  void pwm() {
    analogWrite(ignaiter1_pin, duty);
    analogWrite(ignaiter2_pin, duty);
  }

  void ignition() {
    previousMillis = millis();
    while(millis() - previousMillis < ontime){
      float dI = 0;
      for (int i; i < 255; i++) {
        duty = 0.25*i - (k*dI)/(63.75*1000);
        pwm();
        float I = INA.getCurrent_mA();
        dI = I0*i/255 - I;
        Serial.print(INA.getBusVoltage(), 3);
        Serial.print("\t");
        Serial.print(INA.getCurrent_mA(), 8);
        Serial.print("\t");
        Serial.print(dI, 8);
        Serial.print("\t");        
        Serial.println();
        delay(4);       
      }
      duty = 0.25*255 - (k*dI)/(63.75*1000);
      pwm();
      float I = INA.getCurrent_mA();
      dI = I0 - I;
      Serial.print(INA.getBusVoltage(), 3);
      Serial.print("\t");
      Serial.print(INA.getCurrent_mA(), 8);
      Serial.print("\t");
      Serial.println();
    }
    duty = 0;
    pwm();
  }
};

igniter IGN(ign1_pin, ign2_pin, OnTime);

void setup() {
  Serial.begin(115200);
  pinMode(alart_pin, OUTPUT);

  delay(1000);

  Wire.begin();
  if (!INA.begin() )
  {
    Serial.println("could not connect. Fix and Reboot");
    while(1);
  }
  INA.setMaxCurrentShunt(1, 0.002);
}

void loop() {
  if (ver == 1) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > interval) {
      previousMillis = currentMillis;
      if (State == LOW) {
        State = HIGH;
      } else {
        State = LOW;
      }
      digitalWrite(alart_pin, State);
      Serial.println(currentMillis - previousMillis);
    }
  }

  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "launch") {
      digitalWrite(alart_pin, HIGH);
      delay(500);
      digitalWrite(alart_pin, LOW);
      Serial.println("許可");
      ver = 1;
    } else if (command == "check") {
      Serial.println("確認");
      delay(500);
      Serial.println("success");
    } else if (command == "i") {
      Serial.println("発射");
      IGN.ignition();
      ver = 0;
    } else {
      ver = 0;
    }
  }
  // Serial.println("\nBUS\tSHUNT\tCURRENT\tPOWER");
  Serial.print(INA.getBusVoltage(), 3);
  Serial.print("\t");
  //Serial.print(INA.getShuntVoltage_mV(), 3);
  //Serial.print("\t");
  Serial.print(INA.getCurrent_mA(), 8);
  Serial.print("\t");
  //Serial.print(INA.getPower_mW(), 3);
  Serial.println();
  //delay(1);
}