#include <Arduino.h>

constexpr uint8_t doorSensorPin = 2;
constexpr uint8_t buzzerPin = 8;

void setup() {
  pinMode(doorSensorPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
}

void loop() {
  const bool doorOpen = digitalRead(doorSensorPin) == HIGH;
  digitalWrite(buzzerPin, doorOpen ? HIGH : LOW);
}
