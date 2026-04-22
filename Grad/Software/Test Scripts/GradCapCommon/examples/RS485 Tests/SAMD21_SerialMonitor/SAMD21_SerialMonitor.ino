#include "SAMD21_pinmap.h"
#include "SAMD21_RS485Bridge.h"

SAMD21_RS485Bridge rs485Bridge(Serial1, SAMD_RS485_DE_PIN);

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  rs485Bridge.begin(115200);
  rs485Bridge.setReceive();

  Serial.println("SAMD21 RS485 bridge ready");
}

void loop() {
  rs485Bridge.serviceToUsb(Serial);
}