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

/* ===== Configuration ===== */
static const uint32_t USB_BAUD   = 115200;
static const uint32_t RS485_BAUD = 115200;

/* Small transfer buffers */
static uint8_t usbTo485Buf[64];
static uint8_t busToUsbBuf[64];

void setup() {
  Serial.begin(USB_BAUD);
  while (!Serial) {
    delay(10);
  }

  Serial.println();
  Serial.println("SAMD21 RS-485 debug bridge starting...");
  Serial.println("USB Serial <-> RS-485 passthrough enabled");

  /*
    Important:
    Pin 4 (PA16, QT Py SDA label) is being used as RS-485 DE.
    Do not start Wire() while using this bridge.
  */
  rs485_init_samd(&g_rs485, (void*)&Serial1, SAMD_RS485_DE_PIN, RS485_BAUD);

  /*
    These timings are conservative and should be fine for debug text.
    Tighten them later if needed.
  */
  rs485_set_timings(&g_rs485, 50, 50, 50);
  rs485_set_receive(&g_rs485);

  Serial.print("USB baud: ");
  Serial.println(USB_BAUD);
  Serial.print("RS-485 baud: ");
  Serial.println(RS485_BAUD);
  Serial.println("Bridge ready.");
}

void loop() {
  /* =========================
     USB Serial -> RS-485
     ========================= */
  int usbAvail = Serial.available();
  if (usbAvail > 0) {
    size_t count = 0;

    while (Serial.available() > 0 && count < sizeof(usbTo485Buf)) {
      int c = Serial.read();
      if (c < 0) {
        break;
      }
      usbTo485Buf[count++] = (uint8_t)c;
    }

    if (count > 0) {
      rs485_write(&g_rs485, usbTo485Buf, count);
    }
  }

  /* =========================
     RS-485 -> USB Serial
     ========================= */
  int busAvail = rs485_available(&g_rs485);
  if (busAvail > 0) {
    size_t count = 0;

    while (rs485_available(&g_rs485) > 0 && count < sizeof(busToUsbBuf)) {
      int c = rs485_read(&g_rs485);
      if (c < 0) {
        break;
      }
      busToUsbBuf[count++] = (uint8_t)c;
    }

    if (count > 0) {
      Serial.write(busToUsbBuf, count);
      Serial.flush();
    }
  }
}