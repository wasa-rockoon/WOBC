#define RS_PIN 4
#define E_PIN 7
#define DB4_PIN 8
#define DB5_PIN 9
#define DB6_PIN 10
#define DB7_PIN 11

#define SW1_PIN 21
#define SW2_PIN 20

#define raw1 5
#define raw2 6
#define raw3 18
#define raw4 19

#define col1 26
#define col2 27
#define col3 28
#define col4 29

bool SW1;
bool SW2;

#include <LiquidCrystal.h>

LiquidCrystal lcd(RS_PIN, E_PIN, DB4_PIN, DB5_PIN, DB6_PIN, DB7_PIN);

void setup() {
  SW1 = 1;
  SW2 = 1;
  pinMode(SW1_PIN, INPUT);
  pinMode(SW2_PIN, INPUT);

  pinMode(raw1, OUTPUT);
  pinMode(raw2, OUTPUT);
  pinMode(raw3, OUTPUT);
  pinMode(raw4, OUTPUT);

  pinMode(col1, OUTPUT);
  pinMode(col2, OUTPUT);
  pinMode(col3, OUTPUT);
  pinMode(col4, OUTPUT);

  digitalWrite(col1, HIGH);
  digitalWrite(col2, HIGH);
  digitalWrite(col3, HIGH);
  digitalWrite(col4, HIGH);

  digitalWrite(raw1, LOW);
  digitalWrite(raw2, LOW);
  digitalWrite(raw3, LOW);
  digitalWrite(raw4, LOW);

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