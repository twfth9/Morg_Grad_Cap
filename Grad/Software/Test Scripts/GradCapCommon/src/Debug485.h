/*
 * File: Debug485.h
 * Description: ESP32 RS-485-backed debug print wrapper that behaves like a Serial/Print object and forwards diagnostic text over the shared RS-485 bus.
 * Function count: 15 public methods plus 5 private helper methods
 * Target microcontroller: ESP32
 */

#pragma once

#include <Arduino.h>
#include "HatPins.h"

class Debug485Class : public Print {
public:
  /* Construct a Debug485 transport around the selected UART port.
     Input: HardwareSerial reference to use for RS-485 traffic.
     Output: New Debug485Class object. */
  explicit Debug485Class(HardwareSerial& port = Serial1);

  // Uses default board pinmap from HatPins.h and default timings of 50/50/50 us.
  /* Initialize the RS-485 debug port using the default board pin map and default timing values.
     Input: UART baud rate.
     Output: None. */
  void begin(uint32_t baud = 115200);

  // Uses default board pinmap from HatPins.h with explicit timings.
  /* Initialize the RS-485 debug port using the default board pin map and caller-supplied timing values.
     Input: UART baud rate plus pre/post/turnaround timing in microseconds.
     Output: None. */
  void begin(uint32_t baud,
             uint16_t pre_tx_us,
             uint16_t post_tx_us,
             uint16_t turnaround_us);

  // Fully explicit configuration.
  /* Initialize the RS-485 debug port using the default board pin map and caller-supplied timing values.
     Input: UART baud rate plus pre/post/turnaround timing in microseconds.
     Output: None. */
  void begin(uint32_t baud,
             int de_pin,
             int rx_pin,
             int tx_pin,
             uint16_t pre_tx_us = 50,
             uint16_t post_tx_us = 50,
             uint16_t turnaround_us = 50);

  /* Update DE timing delays used around each transmit operation.
     Input: Pre-TX, post-TX, and turnaround delays in microseconds.
     Output: None. */
  void setTimings(uint16_t pre_tx_us,
                  uint16_t post_tx_us,
                  uint16_t turnaround_us);

  /* Set a prefix string that is automatically inserted at the start of each new text line.
     Input: Null-terminated prefix string pointer.
     Output: None. */
  void setPrefix(const char* prefix);
  /* Disable automatic line prefixes for future output.
     Input: None.
     Output: None. */
  void clearPrefix();

  /* Force any buffered text to be transmitted immediately.
     Input: None.
     Output: None. */
  void flush();
  /* Discard any buffered outgoing text and reset line-start tracking.
     Input: None.
     Output: None. */
  void clear();

  /* Report how many received bytes are waiting in the UART receive buffer.
     Input: None.
     Output: Byte count, or 0 if not started. */
  int available();
  /* Read one received byte from the UART receive buffer.
     Input: None.
     Output: Next byte as an int, or -1 if unavailable. */
  int read();
  /* Peek at the next received byte without removing it from the UART receive buffer.
     Input: None.
     Output: Next byte as an int, or -1 if unavailable. */
  int peek();

  /* Forward all pending received bytes to another Stream, such as USB Serial.
     Input: Destination Stream reference.
     Output: None. */
  void flushRxTo(Stream& out = Serial);

  /* Format and queue a printf-style message for transmission.
     Input: printf format string plus arguments.
     Output: Number of bytes queued for transmission. */
  size_t printf(const char* fmt, ...)
      __attribute__((format(printf, 2, 3)));

  /* Queue one byte for line-buffered debug transmission.
     Input: Single byte.
     Output: Number of bytes accepted. */
  virtual size_t write(uint8_t b) override;
  /* Queue a block of bytes for line-buffered debug transmission.
     Input: Pointer to source data and byte count.
     Output: Number of bytes accepted. */
  virtual size_t write(const uint8_t* buffer, size_t size) override;

  using Print::write;

  // Binary-safe transmit path for framed bus traffic.
  /* Send a raw binary block immediately without line buffering or prefix insertion.
     Input: Pointer to source data and byte count.
     Output: None. */
  void writeRaw(const uint8_t* data, size_t len);

private:
  static constexpr size_t TX_BUF_SIZE = 256;

  HardwareSerial* _serial;
  bool _begun;
  const char* _prefix;

  int _dePin;
  int _rxPin;
  int _txPin;

  uint16_t _preTxUs;
  uint16_t _postTxUs;
  uint16_t _turnaroundUs;

  char _txBuf[TX_BUF_SIZE];
  size_t _txLen;
  bool _atLineStart;

  void configureBus(uint32_t baud, int de_pin, int rx_pin, int tx_pin);
  void setReceive();
  void setTransmit();
  void sendRaw(const uint8_t* data, size_t len);
  void flushBuffer();
  void appendChar(char c);
};

extern Debug485Class Debug485;
