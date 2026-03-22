#pragma once

#include <Arduino.h>

class Debug485Class : public Print {
public:
  explicit Debug485Class(HardwareSerial& port = Serial1);

  // Uses default pinmap from esp32_pinmap.h and default timings of 50/50/50 us.
  void begin(uint32_t baud = 115200);

  // Uses default pinmap from esp32_pinmap.h with explicit timings.
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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32_RS485_TX_PIN   (11)  /* UART TX -> THVD1400D DI */
#define ESP32_RS485_RX_PIN   (12)  /* THVD1400D RO -> UART RX */
#define ESP32_RS485_DE_PIN   (21)  /* THVD1400D DE (HIGH=TX, LOW=RX) */

#ifdef __cplusplus
}
#endif
