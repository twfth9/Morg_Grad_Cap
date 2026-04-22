#pragma once

#include <stddef.h>
#include <stdint.h>

#include <Arduino.h>
#include "SAMD21_Pinmap.h"

/*
 * ============================================================================
 * File: SAMD21_BusPhy.h
 * Project: Grad Cap Display Bus Protocol
 * Target MCU: SAMD21 master
 *
 * Description
 * -----------
 * Low-level RS-485 physical transport interface for the SAMD21 master.
 * ============================================================================
 */

typedef struct {
  uint32_t baud;
  int de_pin;
  uint16_t pre_tx_delay_us;
  uint16_t post_tx_delay_us;
  uint16_t turnaround_delay_us;
} samd21_bus_phy_config_t;

class SAMD21_BusPhy {
 public:
  explicit SAMD21_BusPhy(HardwareSerial& uart);

  void begin();
  void begin(uint32_t baud);
  void begin(uint32_t baud, int de_pin);
  void begin(const samd21_bus_phy_config_t& config);

  bool isInitialized() const;

  void setTiming(uint16_t pre_tx_delay_us,
                 uint16_t post_tx_delay_us,
                 uint16_t turnaround_delay_us);
  uint16_t getPreTxDelayUs() const;
  uint16_t getPostTxDelayUs() const;
  uint16_t getTurnaroundDelayUs() const;

  void setReceiveMode();
  void setTransmitMode();
  bool isInReceiveMode() const;
  bool isInTransmitMode() const;

  int available() const;
  int read();
  int peek();
  size_t readBytes(uint8_t* out, size_t max_len);

  size_t write(uint8_t value);
  size_t write(const uint8_t* data, size_t len);
  void flush();

  int getDePin() const;
  uint32_t getBaudRate() const;

  static samd21_bus_phy_config_t defaultConfig();

 private:
  void beginRawTransmit();
  void endRawTransmit();

  HardwareSerial* uart_;
  int de_pin_;
  uint32_t baud_;
  uint16_t pre_tx_delay_us_;
  uint16_t post_tx_delay_us_;
  uint16_t turnaround_delay_us_;
  bool initialized_;
  bool transmit_mode_;
};
