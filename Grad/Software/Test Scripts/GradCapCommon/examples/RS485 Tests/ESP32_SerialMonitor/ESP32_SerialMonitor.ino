#include <Arduino.h>
#include "Debug485.h"

void setup() {
  delay(500);

  Debug485.begin(115200);
  Debug485.setPrefix("[ESP32] ");

  Debug485.println();
  Debug485.println("Debug485 online");
  Debug485.println("This text should appear on the PC through the SAMD21 bridge.");
  Debug485.print("Chip model: ");
  Debug485.println(ESP.getChipModel());
}

void loop() {
  static uint32_t last_ms = 0;
  static uint32_t counter = 0;

  if (millis() - last_ms >= 1000) {
    last_ms = millis();

    Debug485.print("Heartbeat ");
    Debug485.print(counter++);
    Debug485.print(", free heap = ");
    Debug485.println(ESP.getFreeHeap());
  }
}