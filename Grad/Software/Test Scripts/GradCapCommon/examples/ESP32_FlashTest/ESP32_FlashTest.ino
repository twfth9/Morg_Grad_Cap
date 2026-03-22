#include <Arduino.h>
#include "HatPins.h"
#include "HatSpiMux.h"
#include "Debug485.h"

void setup() {
  Debug485.begin(115200);
  Debug485.setPrefix("[MUXTEST] ");

  hat_spi_mux_begin();

  Debug485.printf("Selecting FLASH and holding it there...\r\n");
  hat_spi_mux_select_flash();
}

void loop() {
  // do nothing
}