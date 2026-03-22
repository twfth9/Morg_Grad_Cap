#include "Debug485.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

Debug485Class::Debug485Class(HardwareSerial& port)
    : _serial(&port),
      _begun(false),
      _prefix(nullptr),
      _dePin(-1),
      _rxPin(-1),
      _txPin(-1),
      _preTxUs(50),
      _postTxUs(50),
      _turnaroundUs(50),
      _txLen(0),
      _atLineStart(true) {}

void Debug485Class::configureBus(uint32_t baud, int de_pin, int rx_pin, int tx_pin) {
  _dePin = de_pin;
  _rxPin = rx_pin;
  _txPin = tx_pin;

  pinMode(_dePin, OUTPUT);
  digitalWrite(_dePin, LOW);

  _serial->begin(baud, SERIAL_8N1, _rxPin, _txPin);
  delay(10);
}

void Debug485Class::setReceive() {
  if (_dePin < 0) return;
  digitalWrite(_dePin, LOW);
  delayMicroseconds(_turnaroundUs);
}

void Debug485Class::setTransmit() {
  if (_dePin < 0) return;
  digitalWrite(_dePin, HIGH);
  delayMicroseconds(_preTxUs);
}

void Debug485Class::begin(uint32_t baud) {
  begin(baud,
        ESP32_RS485_DE_PIN,
        ESP32_RS485_RX_PIN,
        ESP32_RS485_TX_PIN,
        50,
        50,
        50);
}

void Debug485Class::begin(uint32_t baud,
                          uint16_t pre_tx_us,
                          uint16_t post_tx_us,
                          uint16_t turnaround_us) {
  begin(baud,
        ESP32_RS485_DE_PIN,
        ESP32_RS485_RX_PIN,
        ESP32_RS485_TX_PIN,
        pre_tx_us,
        post_tx_us,
        turnaround_us);
}

void Debug485Class::begin(uint32_t baud,
                          int de_pin,
                          int rx_pin,
                          int tx_pin,
                          uint16_t pre_tx_us,
                          uint16_t post_tx_us,
                          uint16_t turnaround_us) {
  _preTxUs = pre_tx_us;
  _postTxUs = post_tx_us;
  _turnaroundUs = turnaround_us;

  configureBus(baud, de_pin, rx_pin, tx_pin);
  setReceive();

  _begun = true;
  _txLen = 0;
  _atLineStart = true;
}

void Debug485Class::setTimings(uint16_t pre_tx_us,
                               uint16_t post_tx_us,
                               uint16_t turnaround_us) {
  _preTxUs = pre_tx_us;
  _postTxUs = post_tx_us;
  _turnaroundUs = turnaround_us;
}

void Debug485Class::setPrefix(const char* prefix) {
  _prefix = prefix;
}

void Debug485Class::clearPrefix() {
  _prefix = nullptr;
}

void Debug485Class::clear() {
  _txLen = 0;
  _atLineStart = true;
}

int Debug485Class::available() {
  if (!_begun) return 0;
  return _serial->available();
}

int Debug485Class::read() {
  if (!_begun) return -1;
  return _serial->read();
}

int Debug485Class::peek() {
  if (!_begun) return -1;
  return _serial->peek();
}

void Debug485Class::flushRxTo(Stream& out) {
  while (available() > 0) {
    int c = read();
    if (c >= 0) {
      out.write(static_cast<uint8_t>(c));
    }
  }
}

void Debug485Class::sendRaw(const uint8_t* data, size_t len) {
  if (!_begun || data == nullptr || len == 0) {
    return;
  }

  setTransmit();
  _serial->write(data, len);
  _serial->flush();
  delayMicroseconds(_postTxUs);
  setReceive();
}

void Debug485Class::flushBuffer() {
  if (!_begun || _txLen == 0) {
    return;
  }

  if (_atLineStart && _prefix != nullptr && _prefix[0] != '\0') {
    sendRaw(reinterpret_cast<const uint8_t*>(_prefix), strlen(_prefix));
    _atLineStart = false;
  }

  sendRaw(reinterpret_cast<const uint8_t*>(_txBuf), _txLen);
  _txLen = 0;
}

void Debug485Class::flush() {
  flushBuffer();
}

void Debug485Class::appendChar(char c) {
  if (_txLen >= TX_BUF_SIZE - 1) {
    flushBuffer();
  }

  if (_atLineStart && _prefix != nullptr && _prefix[0] != '\0') {
    sendRaw(reinterpret_cast<const uint8_t*>(_prefix), strlen(_prefix));
    _atLineStart = false;
  }

  _txBuf[_txLen++] = c;

  if (c == '\n') {
    sendRaw(reinterpret_cast<const uint8_t*>(_txBuf), _txLen);
    _txLen = 0;
    _atLineStart = true;
  }
}

size_t Debug485Class::write(uint8_t b) {
  if (!_begun) return 0;
  appendChar(static_cast<char>(b));
  return 1;
}

size_t Debug485Class::write(const uint8_t* buffer, size_t size) {
  if (!_begun || buffer == nullptr || size == 0) {
    return 0;
  }

  for (size_t i = 0; i < size; ++i) {
    appendChar(static_cast<char>(buffer[i]));
  }

  return size;
}

size_t Debug485Class::printf(const char* fmt, ...) {
  if (!_begun || fmt == nullptr) {
    return 0;
  }

  char temp[256];

  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(temp, sizeof(temp), fmt, args);
  va_end(args);

  if (len <= 0) {
    return 0;
  }

  size_t toWrite = (len < static_cast<int>(sizeof(temp)))
                       ? static_cast<size_t>(len)
                       : (sizeof(temp) - 1);
  return write(reinterpret_cast<const uint8_t*>(temp), toWrite);
}

Debug485Class Debug485;
