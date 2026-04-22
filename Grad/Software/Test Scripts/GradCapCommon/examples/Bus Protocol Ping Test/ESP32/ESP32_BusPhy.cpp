#include "ESP32_BusPhy.h"

ESP32_BusPhy::ESP32_BusPhy(HardwareSerial& uart)
    : uart_(&uart),
      de_pin_(ESP32_PIN_RS485_DE),
      rx_pin_(ESP32_PIN_RS485_RX),
      tx_pin_(ESP32_PIN_RS485_TX),
      baud_(0),
      pre_tx_delay_us_(0),
      post_tx_delay_us_(0),
      turnaround_delay_us_(0),
      initialized_(false),
      transmit_mode_(false) {}

esp32_bus_phy_config_t ESP32_BusPhy::defaultConfig() {
  esp32_bus_phy_config_t cfg{};
  cfg.baud = 460800;
  cfg.de_pin = ESP32_PIN_RS485_DE;
  cfg.rx_pin = ESP32_PIN_RS485_RX;
  cfg.tx_pin = ESP32_PIN_RS485_TX;
  cfg.pre_tx_delay_us = 50;
  cfg.post_tx_delay_us = 50;
  cfg.turnaround_delay_us = 50;
  return cfg;
}

void ESP32_BusPhy::begin() {
  begin(defaultConfig());
}

void ESP32_BusPhy::begin(uint32_t baud) {
  auto cfg = defaultConfig();
  cfg.baud = baud;
  begin(cfg);
}

void ESP32_BusPhy::begin(uint32_t baud, int de_pin, int rx_pin, int tx_pin) {
  auto cfg = defaultConfig();
  cfg.baud = baud;
  cfg.de_pin = de_pin;
  cfg.rx_pin = rx_pin;
  cfg.tx_pin = tx_pin;
  begin(cfg);
}

void ESP32_BusPhy::begin(const esp32_bus_phy_config_t& config) {
  de_pin_ = config.de_pin;
  rx_pin_ = config.rx_pin;
  tx_pin_ = config.tx_pin;
  baud_ = config.baud;
  pre_tx_delay_us_ = config.pre_tx_delay_us;
  post_tx_delay_us_ = config.post_tx_delay_us;
  turnaround_delay_us_ = config.turnaround_delay_us;

  pinMode(de_pin_, OUTPUT);
  digitalWrite(de_pin_, LOW);
  uart_->begin(baud_, SERIAL_8N1, rx_pin_, tx_pin_);
  setReceiveMode();
  initialized_ = true;
}

bool ESP32_BusPhy::isInitialized() const { return initialized_; }

void ESP32_BusPhy::setTiming(uint16_t pre_tx_delay_us,
                             uint16_t post_tx_delay_us,
                             uint16_t turnaround_delay_us) {
  pre_tx_delay_us_ = pre_tx_delay_us;
  post_tx_delay_us_ = post_tx_delay_us;
  turnaround_delay_us_ = turnaround_delay_us;
}

uint16_t ESP32_BusPhy::getPreTxDelayUs() const { return pre_tx_delay_us_; }
uint16_t ESP32_BusPhy::getPostTxDelayUs() const { return post_tx_delay_us_; }
uint16_t ESP32_BusPhy::getTurnaroundDelayUs() const { return turnaround_delay_us_; }

void ESP32_BusPhy::setReceiveMode() {
  if (de_pin_ >= 0) digitalWrite(de_pin_, LOW);
  transmit_mode_ = false;
}

void ESP32_BusPhy::setTransmitMode() {
  if (de_pin_ >= 0) digitalWrite(de_pin_, HIGH);
  transmit_mode_ = true;
}

bool ESP32_BusPhy::isInReceiveMode() const { return initialized_ && !transmit_mode_; }
bool ESP32_BusPhy::isInTransmitMode() const { return initialized_ && transmit_mode_; }

int ESP32_BusPhy::available() const {
  return initialized_ ? uart_->available() : 0;
}

int ESP32_BusPhy::read() {
  return initialized_ ? uart_->read() : -1;
}

int ESP32_BusPhy::peek() {
  return initialized_ ? uart_->peek() : -1;
}

size_t ESP32_BusPhy::readBytes(uint8_t* out, size_t max_len) {
  if (!initialized_ || out == nullptr || max_len == 0) return 0;
  size_t count = 0;
  while (count < max_len && uart_->available() > 0) {
    int value = uart_->read();
    if (value < 0) break;
    out[count++] = static_cast<uint8_t>(value);
  }
  return count;
}

size_t ESP32_BusPhy::write(uint8_t value) {
  return write(&value, 1);
}

size_t ESP32_BusPhy::write(const uint8_t* data, size_t len) {
  if (!initialized_ || data == nullptr || len == 0) return 0;
  beginRawTransmit();
  size_t written = uart_->write(data, len);
  endRawTransmit();
  return written;
}

void ESP32_BusPhy::flush() {
  if (initialized_) uart_->flush();
}

int ESP32_BusPhy::getDePin() const { return de_pin_; }
int ESP32_BusPhy::getRxPin() const { return rx_pin_; }
int ESP32_BusPhy::getTxPin() const { return tx_pin_; }
uint32_t ESP32_BusPhy::getBaudRate() const { return baud_; }

void ESP32_BusPhy::beginRawTransmit() {
  setTransmitMode();
  if (pre_tx_delay_us_ > 0) delayMicroseconds(pre_tx_delay_us_);
}

void ESP32_BusPhy::endRawTransmit() {
  uart_->flush();
  if (post_tx_delay_us_ > 0) delayMicroseconds(post_tx_delay_us_);
  setReceiveMode();
  if (turnaround_delay_us_ > 0) delayMicroseconds(turnaround_delay_us_);
}
