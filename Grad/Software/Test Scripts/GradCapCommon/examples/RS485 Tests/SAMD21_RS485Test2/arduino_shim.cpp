#include <Arduino.h>
#include "arduino_shim.h"

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

} // extern "C"