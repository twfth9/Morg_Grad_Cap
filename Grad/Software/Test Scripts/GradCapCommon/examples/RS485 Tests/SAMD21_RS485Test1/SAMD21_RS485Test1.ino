#include <Arduino.h>
#include "samd21_pinmap.h"
#include "rs485_halfduplex_samd.h"

/* ===== C-callable Arduino wrappers ===== */
extern "C" {
  void a_pinMode(int pin, int mode) { pinMode(pin, mode); }
  void a_digitalWrite(int pin, int val) { digitalWrite(pin, val); }
  void a_delay(int ms) { delay(ms); }
  void a_delayMicroseconds(unsigned int us) { delayMicroseconds(us); }

  void a_serial_begin(void* serial_port, uint32_t baud) {
    HardwareSerial* s = (HardwareSerial*)serial_port;
    s->begin(baud);
  }
  size_t a_serial_write(void* serial_port, const uint8_t* data, size_t len) {
    HardwareSerial* s = (HardwareSerial*)serial_port;
    return s->write(data, len);
  }
  void a_serial_flush(void* serial_port) {
    HardwareSerial* s = (HardwareSerial*)serial_port;
    s->flush();
  }
  int a_serial_available(void* serial_port) {
    HardwareSerial* s = (HardwareSerial*)serial_port;
    return s->available();
  }
  int a_serial_read(void* serial_port) {
    HardwareSerial* s = (HardwareSerial*)serial_port;
    return s->read();
  }
  int a_serial_peek(void* serial_port) {
    HardwareSerial* s = (HardwareSerial*)serial_port;
    return s->peek();
  }
}

/* ===== RS-485 context ===== */
static rs485_t g_rs485;

/* line buffer */
static char lineBuf[128];
static size_t lineLen = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println("SAMD21 RS-485 (C-style) test starting...");

  /* Important: pin 4 is labeled SDA but we are using it for DE, so don't start Wire() here. */

  rs485_init_samd(&g_rs485, (void*)&Serial1, SAMD_RS485_DE_PIN, 9600);
  rs485_set_timings(&g_rs485, 50, 50, 50);
  rs485_set_receive(&g_rs485);

  Serial.println("Waiting for RS-485 lines from ESP32...");
}

void loop() {
  while (rs485_available(&g_rs485) > 0) {
    int c = rs485_read(&g_rs485);
    if (c < 0) break;

    if (c == '\n') {
      if (lineLen > 0 && lineBuf[lineLen - 1] == '\r') {
        lineLen--;
      }
      lineBuf[lineLen] = '\0';

      Serial.print("RX: ");
      Serial.println(lineBuf);

      lineLen = 0;
    } else {
      if (lineLen < sizeof(lineBuf) - 1) {
        lineBuf[lineLen++] = (char)c;
      } else {
        lineLen = 0; // overflow, reset
      }
    }
  }
}