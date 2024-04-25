#define RS_PIN 4
#define E_PIN 7
#define DB4_PIN 8
#define DB5_PIN 9
#define DB6_PIN 10
#define DB7_PIN 11

#define SW1_PIN 16
#define SW2_PIN 17

bool SW1;
bool SW2;

#include <LiquidCrystal.h>

LiquidCrystal lcd(RS_PIN, E_PIN, DB4_PIN, DB5_PIN, DB6_PIN, DB7_PIN);

void setup() {
  SW1 = 1;
  SW2 = 1;
  pinMode(SW1_PIN, INPUT);
  pinMode(SW2_PIN, INPUT);
  lcd.begin(20,4);
  lcd.print("START");
  delay(3000);
}

void loop() {
  if (digitalRead(SW1_PIN) == LOW) {
    SW1=0;
  } else {
    SW1=1;
  }
  if (digitalRead(SW2_PIN) == HIGH) {
    SW2=(SW2+1)%2;
  }
  while(digitalRead(SW2_PIN) == HIGH);

  lcd.setCursor(0, 0);
  if(SW2){
    if(SW1) lcd.print("location 1");
    else lcd.print("location 2");
  }else{
    lcd.print("condition  ");
  }

  delay(100);
}