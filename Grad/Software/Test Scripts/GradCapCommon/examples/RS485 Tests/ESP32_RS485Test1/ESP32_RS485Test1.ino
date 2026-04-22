#include <Arduino.h>
#include "esp32_pinmap.h"
#include "rs485_halfduplex.h"

static rs485_t g_rs485;
static uint32_t g_count = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("ESP32 RS-485 test starting...");

  rs485_init_esp32(&g_rs485,
                   (void*)&Serial1,
                   ESP32_RS485_DE_PIN,
                   9600,
                   ESP32_RS485_RX_PIN,
                   ESP32_RS485_TX_PIN);

  rs485_set_receive(&g_rs485);
}

void loop() {
  char msg[96];
  snprintf(msg, sizeof(msg), "HELLO_FROM_ESP32 count=%lu", (unsigned long)g_count++);
  rs485_println(&g_rs485, msg);
  delay(1000);
}