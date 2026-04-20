/*
 * File: SAMD21_RS485Bridge.h
 * Description: Simple half-duplex RS-485 helper for the SAMD21 bridge microcontroller. Wraps a HardwareSerial port and DE timing so the SAMD can relay traffic to USB.
 * Function count: 12 public methods
 * Target microcontroller: SAMD21
 */

#pragma once

#include <Arduino.h>

class SAMD21_RS485Bridge {
public:
  /* Construct an RS-485 bridge around the selected UART and DE pin.
     Input: HardwareSerial reference and DE GPIO number.
     Output: New bridge object. */
  SAMD21_RS485Bridge(HardwareSerial& rs485Port, int dePin);

  /* Initialize the RS-485 UART and place the transceiver in receive mode.
     Input: UART baud rate.
     Output: None. */
  void begin(uint32_t baud);
  /* Update the DE timing delays used around each transmission.
     Input: Pre-TX, post-TX, and turnaround delays in microseconds.
     Output: None. */
  void setTimings(uint16_t preTxUs, uint16_t postTxUs, uint16_t turnaroundUs);

  /* Force the transceiver into receive mode.
     Input: None.
     Output: None. */
  void setReceive();
  /* Force the transceiver into transmit mode.
     Input: None.
     Output: None. */
  void setTransmit();

  /* Report the number of bytes waiting in the UART receive buffer.
     Input: None.
     Output: Byte count. */
  int available();
  /* Read one byte from the UART receive buffer.
     Input: None.
     Output: Next byte as an int, or -1 if unavailable. */
  int read();
  /* Peek at the next byte in the UART receive buffer.
     Input: None.
     Output: Next byte as an int, or -1 if unavailable. */
  int peek();

  /* Transmit a block of bytes over the RS-485 bus.
     Input: Pointer to source bytes and byte count.
     Output: Number of bytes written. */
  size_t write(const uint8_t* data, size_t len);
  /* Transmit one byte over the RS-485 bus.
     Input: Single byte.
     Output: Number of bytes written. */
  size_t write(uint8_t b);
  /* Transmit a null-terminated C string over the RS-485 bus.
     Input: String pointer.
     Output: Number of bytes written. */
  size_t print(const char* s);
  /* Transmit a C string followed by CRLF over the RS-485 bus.
     Input: String pointer.
     Output: Number of bytes written. */
  size_t println(const char* s);

  /* Forward all currently received RS-485 bytes to a USB-facing Stream.
     Input: Destination Stream reference.
     Output: None. */
  void serviceToUsb(Stream& usb);
  /* Copy available received bytes into a caller-supplied buffer.
     Input: Destination buffer pointer and maximum byte count.
     Output: Number of bytes copied. */
  size_t readBytes(uint8_t* buffer, size_t maxLen);

private:
  HardwareSerial* _port;
  int _dePin;
  uint16_t _preTxUs;
  uint16_t _postTxUs;
  uint16_t _turnaroundUs;
};
