/*
 * File: SAMD21_RS485Bridge.cpp
 * Description: Implementation of the SAMD21 RS-485 bridge helper. Manages DE direction control, write timing, and forwarding received bytes to a USB stream.
 * Function count: 12 methods
 * Target microcontroller: SAMD21
 */

#include "SAMD21_RS485Bridge.h"

SAMD21_RS485Bridge::SAMD21_RS485Bridge(HardwareSerial& rs485Port, int dePin)
    : _port(&rs485Port),
      _dePin(dePin),
      _preTxUs(50),
      _postTxUs(50),
      _turnaroundUs(50) {
}

/* Initialize the SAMD21-side RS-485 UART and default to receive mode. */
void SAMD21_RS485Bridge::begin(uint32_t baud) {
  pinMode(_dePin, OUTPUT);
  digitalWrite(_dePin, LOW);   // receive mode by default

  _port->begin(baud);
  delay(10);
}

/* Update the DE timing values used around each bridge transmission. */
void SAMD21_RS485Bridge::setTimings(uint16_t preTxUs, uint16_t postTxUs, uint16_t turnaroundUs) {
  _preTxUs = preTxUs;
  _postTxUs = postTxUs;
  _turnaroundUs = turnaroundUs;
}

/* Force the transceiver into receive mode and allow turnaround time. */
void SAMD21_RS485Bridge::setReceive() {
  digitalWrite(_dePin, LOW);
  delayMicroseconds(_turnaroundUs);
}

/* Force the transceiver into transmit mode and wait the configured pre-TX delay. */
void SAMD21_RS485Bridge::setTransmit() {
  digitalWrite(_dePin, HIGH);
  delayMicroseconds(_preTxUs);
}

/* Return the number of bytes waiting in the UART receive buffer. */
int SAMD21_RS485Bridge::available() {
  return _port->available();
}

/* Read one byte from the UART receive buffer. */
int SAMD21_RS485Bridge::read() {
  return _port->read();
}

/* Peek at the next byte in the UART receive buffer. */
int SAMD21_RS485Bridge::peek() {
  return _port->peek();
}

/* Transmit a raw block of bytes over the RS-485 bus. */
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

/* Transmit one byte over the RS-485 bus. */
size_t SAMD21_RS485Bridge::write(uint8_t b) {
  return write(&b, 1);
}

/* Transmit a null-terminated C string over the RS-485 bus. */
size_t SAMD21_RS485Bridge::print(const char* s) {
  if (!s) {
    return 0;
  }
  return write(reinterpret_cast<const uint8_t*>(s), strlen(s));
}

/* Transmit a C string followed by CRLF over the RS-485 bus. */
size_t SAMD21_RS485Bridge::println(const char* s) {
  size_t n = 0;
  if (s) {
    n += print(s);
  }
  n += write(reinterpret_cast<const uint8_t*>("\r\n"), 2);
  return n;
}

/* Forward all currently received RS-485 bytes to a USB-facing stream. */
void SAMD21_RS485Bridge::serviceToUsb(Stream& usb) {
  while (_port->available() > 0) {
    int c = _port->read();
    if (c < 0) {
      break;
    }
    usb.write(static_cast<uint8_t>(c));
  }
}

/* Copy as many currently available received bytes as will fit in the caller buffer. */
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
