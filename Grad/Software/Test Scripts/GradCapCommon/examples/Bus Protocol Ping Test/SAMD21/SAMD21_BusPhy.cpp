#include "SAMD21_BusPhy.h"

SAMD21_BusPhy::SAMD21_BusPhy(HardwareSerial& uart)
    : uart_(&uart),
      de_pin_(SAMD21_PIN_RS485_DE),
      baud_(0),
      pre_tx_delay_us_(0),
      post_tx_delay_us_(0),
      turnaround_delay_us_(0),
      initialized_(false),
      transmit_mode_(false) {}

samd21_bus_phy_config_t SAMD21_BusPhy::defaultConfig() {
  samd21_bus_phy_config_t cfg{};
  cfg.baud = 460800;
  cfg.de_pin = SAMD21_PIN_RS485_DE;
  cfg.pre_tx_delay_us = 50;
  cfg.post_tx_delay_us = 50;
  cfg.turnaround_delay_us = 50;
  return cfg;
}

void SAMD21_BusPhy::begin() {
  begin(defaultConfig());
}

void SAMD21_BusPhy::begin(uint32_t baud) {
  auto cfg = defaultConfig();
  cfg.baud = baud;
  begin(cfg);
}

void SAMD21_BusPhy::begin(uint32_t baud, int de_pin) {
  auto cfg = defaultConfig();
  cfg.baud = baud;
  cfg.de_pin = de_pin;
  begin(cfg);
}

void SAMD21_BusPhy::begin(const samd21_bus_phy_config_t& config) {
  de_pin_ = config.de_pin;
  baud_ = config.baud;
  pre_tx_delay_us_ = config.pre_tx_delay_us;
  post_tx_delay_us_ = config.post_tx_delay_us;
  turnaround_delay_us_ = config.turnaround_delay_us;

  pinMode(de_pin_, OUTPUT);
  digitalWrite(de_pin_, LOW);
  uart_->begin(baud_);
  setReceiveMode();
  initialized_ = true;
}

bool SAMD21_BusPhy::isInitialized() const { return initialized_; }

void SAMD21_BusPhy::setTiming(uint16_t pre_tx_delay_us,
                              uint16_t post_tx_delay_us,
                              uint16_t turnaround_delay_us) {
  pre_tx_delay_us_ = pre_tx_delay_us;
  post_tx_delay_us_ = post_tx_delay_us;
  turnaround_delay_us_ = turnaround_delay_us;
}

uint16_t SAMD21_BusPhy::getPreTxDelayUs() const { return pre_tx_delay_us_; }
uint16_t SAMD21_BusPhy::getPostTxDelayUs() const { return post_tx_delay_us_; }
uint16_t SAMD21_BusPhy::getTurnaroundDelayUs() const { return turnaround_delay_us_; }

void SAMD21_BusPhy::setReceiveMode() {
  if (de_pin_ >= 0) digitalWrite(de_pin_, LOW);
  transmit_mode_ = false;
}

void SAMD21_BusPhy::setTransmitMode() {
  if (de_pin_ >= 0) digitalWrite(de_pin_, HIGH);
  transmit_mode_ = true;
}

bool SAMD21_BusPhy::isInReceiveMode() const { return initialized_ && !transmit_mode_; }
bool SAMD21_BusPhy::isInTransmitMode() const { return initialized_ && transmit_mode_; }

int SAMD21_BusPhy::available() const {
  return initialized_ ? uart_->available() : 0;
}

int SAMD21_BusPhy::read() {
  return initialized_ ? uart_->read() : -1;
}

int SAMD21_BusPhy::peek() {
  return initialized_ ? uart_->peek() : -1;
}

size_t SAMD21_BusPhy::readBytes(uint8_t* out, size_t max_len) {
  if (!initialized_ || out == nullptr || max_len == 0) return 0;
  size_t count = 0;
  while (count < max_len && uart_->available() > 0) {
    int value = uart_->read();
    if (value < 0) break;
    out[count++] = static_cast<uint8_t>(value);
  }
  return count;
}

size_t SAMD21_BusPhy::write(uint8_t value) {
  return write(&value, 1);
}

size_t SAMD21_BusPhy::write(const uint8_t* data, size_t len) {
  if (!initialized_ || data == nullptr || len == 0) return 0;
  beginRawTransmit();
  size_t written = uart_->write(data, len);
  endRawTransmit();
  return written;
}

void SAMD21_BusPhy::flush() {
  if (initialized_) uart_->flush();
}

int SAMD21_BusPhy::getDePin() const { return de_pin_; }
uint32_t SAMD21_BusPhy::getBaudRate() const { return baud_; }

void SAMD21_BusPhy::beginRawTransmit() {
  setTransmitMode();
  if (pre_tx_delay_us_ > 0) delayMicroseconds(pre_tx_delay_us_);
}

void SAMD21_BusPhy::endRawTransmit() {
  uart_->flush();
  if (post_tx_delay_us_ > 0) delayMicroseconds(post_tx_delay_us_);
  setReceiveMode();
  if (turnaround_delay_us_ > 0) delayMicroseconds(turnaround_delay_us_);
}
