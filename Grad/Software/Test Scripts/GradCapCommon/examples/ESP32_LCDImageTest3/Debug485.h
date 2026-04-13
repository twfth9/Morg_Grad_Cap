#pragma once

#include <Arduino.h>
#include "HatPins.h"

class Debug485Class : public Print {
public:
  explicit Debug485Class(HardwareSerial& port = Serial1);

  // Uses default board pinmap from HatPins.h and default timings of 50/50/50 us.
  void begin(uint32_t baud = 115200);

  // Uses default board pinmap from HatPins.h with explicit timings.
  void begin(uint32_t baud,
             uint16_t pre_tx_us,
             uint16_t post_tx_us,
             uint16_t turnaround_us);

  // Fully explicit configuration.
  void begin(uint32_t baud,
             int de_pin,
             int rx_pin,
             int tx_pin,
             uint16_t pre_tx_us = 50,
             uint16_t post_tx_us = 50,
             uint16_t turnaround_us = 50);

  void setTimings(uint16_t pre_tx_us,
                  uint16_t post_tx_us,
                  uint16_t turnaround_us);

  void setPrefix(const char* prefix);
  void clearPrefix();

  void flush();
  void clear();

  int available();
  int read();
  int peek();

  void flushRxTo(Stream& out = Serial);

  size_t printf(const char* fmt, ...)
      __attribute__((format(printf, 2, 3)));

  virtual size_t write(uint8_t b) override;
  virtual size_t write(const uint8_t* buffer, size_t size) override;

  using Print::write;

  // Binary-safe transmit path for framed bus traffic.
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