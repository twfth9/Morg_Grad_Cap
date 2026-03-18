#include <Arduino.h>

void setup() {
  pinMode(5, OUTPUT);
  analogWriteResolution(8);
}

void loop() {
  //analogWrite(5, 64);   // 25%
  //delay(1000);

  //analogWrite(5, 128);  // 50%
  //delay(1000);

  //analogWrite(5, 192);  // 75%
  //delay(1000);

  analogWrite(5, 255);  // 100%
  delay(1000);

  //analogWrite(5, 0);    // 0%
  //delay(1000);
}