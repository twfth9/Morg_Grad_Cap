#pragma once

#include <stddef.h>
#include <stdint.h>

#include <Arduino.h>
#include "ESP32_Pinmap.h"

/*
 * ============================================================================
 * File: ESP32_BusPhy.h
 * Project: Grad Cap Display Bus Protocol
 * Target MCU: ESP32 display nodes
 *
 * Description
 * -----------
 * Low-level RS-485 physical transport interface for ESP32 nodes.
 *
 * Responsibilities:
 * - initialize UART for RS-485 traffic
 * - manage DE pin for half-duplex direction control
 * - provide raw byte send/receive helpers
 * - hide default pin/timing configuration from application code
 *
 * Non-responsibilities:
 * - protocol framing/parsing
 * - CRC
 * - node/master scheduling
 * - debug line buffering
 * ============================================================================
 */

typedef struct {
  uint32_t baud;
  int de_pin;
  int rx_pin;
  int tx_pin;
  uint16_t pre_tx_delay_us;
  uint16_t post_tx_delay_us;
  uint16_t turnaround_delay_us;
} esp32_bus_phy_config_t;

class ESP32_BusPhy {
 public:
  explicit ESP32_BusPhy(HardwareSerial& uart);

  /* Initialize with built-in defaults. */
  void begin();

  /* Initialize with default pins and explicit baud. */
  void begin(uint32_t baud);

  /* Initialize with explicit baud and pins. Uses default timing. */
  void begin(uint32_t baud, int de_pin, int rx_pin, int tx_pin);

  /* Initialize with fully specified configuration. */
  void begin(const esp32_bus_phy_config_t& config);

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
  int getRxPin() const;
  int getTxPin() const;
  uint32_t getBaudRate() const;

  static esp32_bus_phy_config_t defaultConfig();

 private:
  void beginRawTransmit();
  void endRawTransmit();

  HardwareSerial* uart_;
  int de_pin_;
  int rx_pin_;
  int tx_pin_;
  uint32_t baud_;
  uint16_t pre_tx_delay_us_;
  uint16_t post_tx_delay_us_;
  uint16_t turnaround_delay_us_;
  bool initialized_;
  bool transmit_mode_;
};
