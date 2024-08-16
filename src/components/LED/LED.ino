#include <arduino.h>
#define ESP_LED 42

void setup() {
  pinMode(ESP_LED, OUTPUT);
}

void loop() {
  digitalWrite(ESP_LED, HIGH);
  delay(500);
  digitalWrite(ESP_LED, LOW);
  delay(500);
}