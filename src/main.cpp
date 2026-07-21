#include <Arduino.h>
const int TRIG_PIN = 26;
const int ECHO_PIN = 34;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void loop() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long dur = pulseIn(ECHO_PIN, HIGH, 27000UL);
  float cm = dur ? (dur * 0.0343f) / 2.0f : -1;   // -1 means no echo
  Serial.printf("distance = %.1f cm\n", cm);
  delay(300);
}