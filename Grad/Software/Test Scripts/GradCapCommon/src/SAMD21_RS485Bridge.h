#pragma once

#include <Arduino.h>

class SAMD21_RS485Bridge {
public:
  SAMD21_RS485Bridge(HardwareSerial& rs485Port, int dePin);

  void begin(uint32_t baud);
  void setTimings(uint16_t preTxUs, uint16_t postTxUs, uint16_t turnaroundUs);

  void setReceive();
  void setTransmit();

  int available();
  int read();
  int peek();

  size_t write(const uint8_t* data, size_t len);
  size_t write(uint8_t b);
  size_t print(const char* s);
  size_t println(const char* s);

  void serviceToUsb(Stream& usb);
  size_t readBytes(uint8_t* buffer, size_t maxLen);

private:
  HardwareSerial* _port;
  int _dePin;
  uint16_t _preTxUs;
  uint16_t _postTxUs;
  uint16_t _turnaroundUs;
};