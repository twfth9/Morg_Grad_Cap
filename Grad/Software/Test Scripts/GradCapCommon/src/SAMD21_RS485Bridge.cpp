#include "SAMD21_RS485Bridge.h"

SAMD21_RS485Bridge::SAMD21_RS485Bridge(HardwareSerial& rs485Port, int dePin)
    : _port(&rs485Port),
      _dePin(dePin),
      _preTxUs(50),
      _postTxUs(50),
      _turnaroundUs(50) {
}

void SAMD21_RS485Bridge::begin(uint32_t baud) {
  pinMode(_dePin, OUTPUT);
  digitalWrite(_dePin, LOW);   // receive mode by default

  _port->begin(baud);
  delay(10);
}

void SAMD21_RS485Bridge::setTimings(uint16_t preTxUs, uint16_t postTxUs, uint16_t turnaroundUs) {
  _preTxUs = preTxUs;
  _postTxUs = postTxUs;
  _turnaroundUs = turnaroundUs;
}

void SAMD21_RS485Bridge::setReceive() {
  digitalWrite(_dePin, LOW);
  delayMicroseconds(_turnaroundUs);
}

void SAMD21_RS485Bridge::setTransmit() {
  digitalWrite(_dePin, HIGH);
  delayMicroseconds(_preTxUs);
}

int SAMD21_RS485Bridge::available() {
  return _port->available();
}

int SAMD21_RS485Bridge::read() {
  return _port->read();
}

int SAMD21_RS485Bridge::peek() {
  return _port->peek();
}

size_t SAMD21_RS485Bridge::write(const uint8_t* data, size_t len) {
  if (!data || len == 0) {
    return 0;
  }

  setTransmit();
  size_t written = _port->write(data, len);
  _port->flush();
  delayMicroseconds(_postTxUs);
  setReceive();

  return written;
}

size_t SAMD21_RS485Bridge::write(uint8_t b) {
  return write(&b, 1);
}

size_t SAMD21_RS485Bridge::print(const char* s) {
  if (!s) {
    return 0;
  }
  return write(reinterpret_cast<const uint8_t*>(s), strlen(s));
}

size_t SAMD21_RS485Bridge::println(const char* s) {
  size_t n = 0;
  if (s) {
    n += print(s);
  }
  n += write(reinterpret_cast<const uint8_t*>("\r\n"), 2);
  return n;
}

void SAMD21_RS485Bridge::serviceToUsb(Stream& usb) {
  while (_port->available() > 0) {
    int c = _port->read();
    if (c < 0) {
      break;
    }
    usb.write(static_cast<uint8_t>(c));
  }
}

size_t SAMD21_RS485Bridge::readBytes(uint8_t* buffer, size_t maxLen) {
  if (!buffer || maxLen == 0) {
    return 0;
  }

  size_t count = 0;
  while (count < maxLen && _port->available() > 0) {
    int c = _port->read();
    if (c < 0) {
      break;
    }
    buffer[count++] = static_cast<uint8_t>(c);
  }

  return count;
}