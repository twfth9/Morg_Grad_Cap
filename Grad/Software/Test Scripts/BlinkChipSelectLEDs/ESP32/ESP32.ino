#include <Arduino.h>
#include "HatSpiMux.h"

static uint32_t last_ms = 0;
static uint8_t state = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Hat mux bring-up (C library): cycling 4 states...");

  hat_spi_mux_begin();
  hat_spi_mux_disconnect();
}

void loop() {
  if (millis() - last_ms >= 1000) {
    last_ms = millis();

    switch (state) {
      case 0:
        hat_spi_mux_disconnect();
        Serial.println("State 0: DISCONNECT (Y2 unused): B=1, A=0");
        break;
      case 1:
        hat_spi_mux_select_flash();
        Serial.println("State 1: FLASH (Y0): B=0, A=0");
        break;
      case 2:
        hat_spi_mux_select_sd();
        Serial.println("State 2: SD (Y1): B=0, A=1 (connect+CS via mod)");
        break;
      case 3:
        hat_spi_mux_select_lcd();
        Serial.println("State 3: LCD (Y3): B=1, A=1");
        break;
    }

    state = (state + 1) & 0x03;
  }
}
