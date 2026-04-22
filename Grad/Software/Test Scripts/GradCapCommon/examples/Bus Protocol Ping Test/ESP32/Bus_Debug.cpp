#include "Bus_Debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {
size_t bounded_strlen(const char* text, size_t max_len) {
  if (text == nullptr) return 0;
  size_t len = 0;
  while (len < max_len && text[len] != '\0') ++len;
  return len;
}
}

BusDebug::BusDebug()
    : node_(nullptr),
      enabled_(true),
      level_(BUS_DEBUG_LEVEL_INFO),
      line_length_(0),
      lines_queued_count_(0),
      lines_dropped_count_(0),
      truncated_(false) {
  memset(line_buffer_, 0, sizeof(line_buffer_));
  memset(prefix_, 0, sizeof(prefix_));
}

void BusDebug::begin(BusNode* node) { node_ = node; }
bool BusDebug::isAttached() const { return node_ != nullptr; }
void BusDebug::setEnabled(bool enabled) { enabled_ = enabled; }
bool BusDebug::isEnabled() const { return enabled_; }
void BusDebug::setLevel(uint8_t level) { level_ = level; }
uint8_t BusDebug::getLevel() const { return level_; }

void BusDebug::setPrefix(const char* prefix) {
  memset(prefix_, 0, sizeof(prefix_));
  if (prefix == nullptr) return;
  const size_t len = bounded_strlen(prefix, kPrefixBufferSize - 1);
  if (len > 0) memcpy(prefix_, prefix, len);
  prefix_[len] = '\0';
}

const char* BusDebug::getPrefix() const { return prefix_; }
size_t BusDebug::write(char c) { return appendChar(c); }

size_t BusDebug::write(const uint8_t* data, size_t len) {
  if (!enabled_ || data == nullptr || len == 0) return 0;
  size_t accepted = 0;
  for (size_t i = 0; i < len; ++i) accepted += appendChar(static_cast<char>(data[i]));
  return accepted;
}

size_t BusDebug::print(const char* text) {
  if (!enabled_ || text == nullptr) return 0;
  return write(reinterpret_cast<const uint8_t*>(text), strlen(text));
}

size_t BusDebug::print(int value) { return printNumberSigned(static_cast<long>(value)); }
size_t BusDebug::print(unsigned int value) { return printNumberUnsigned(static_cast<unsigned long>(value)); }
size_t BusDebug::print(long value) { return printNumberSigned(value); }
size_t BusDebug::print(unsigned long value) { return printNumberUnsigned(value); }

size_t BusDebug::println(const char* text) {
  size_t count = 0;
  if (text != nullptr) count += print(text);
  count += println();
  return count;
}

size_t BusDebug::println() { return appendChar('\n'); }
size_t BusDebug::println(int value) { size_t c = print(value); return c + println(); }
size_t BusDebug::println(unsigned int value) { size_t c = print(value); return c + println(); }

int BusDebug::printf(const char* fmt, ...) {
  if (!enabled_ || fmt == nullptr) return 0;
  char temp[128];
  va_list args;
  va_start(args, fmt);
  const int written = vsnprintf(temp, sizeof(temp), fmt, args);
  va_end(args);
  if (written <= 0) return written;
  const size_t to_write = (static_cast<size_t>(written) < sizeof(temp)) ? static_cast<size_t>(written) : (sizeof(temp) - 1);
  write(reinterpret_cast<const uint8_t*>(temp), to_write);
  if (static_cast<size_t>(written) >= sizeof(temp)) truncated_ = true;
  return written;
}

bool BusDebug::flush() { return queueCurrentLine(true); }

void BusDebug::clearLineBuffer() {
  memset(line_buffer_, 0, sizeof(line_buffer_));
  line_length_ = 0;
  truncated_ = false;
}

size_t BusDebug::getLineLength() const { return line_length_; }
bool BusDebug::isLineEmpty() const { return line_length_ == 0; }
uint32_t BusDebug::getLinesQueuedCount() const { return lines_queued_count_; }
uint32_t BusDebug::getLinesDroppedCount() const { return lines_dropped_count_; }
bool BusDebug::wasTruncated() const { return truncated_; }

size_t BusDebug::appendChar(char c) {
  if (!enabled_) return 0;
  if (c == '\r') return 1;
  if (c == '\n') return queueCurrentLine(true) ? 1 : 0;

  if (line_length_ >= (kLineBufferSize - 1)) {
    truncated_ = true;
    queueCurrentLine(true);
  }

  if (line_length_ < (kLineBufferSize - 1)) {
    line_buffer_[line_length_++] = c;
    line_buffer_[line_length_] = '\0';
    return 1;
  }
  return 0;
}

bool BusDebug::queueCurrentLine(bool) {
  if (!enabled_) {
    clearLineBuffer();
    return false;
  }
  if (node_ == nullptr) {
    lines_dropped_count_++;
    clearLineBuffer();
    return false;
  }
  if (line_length_ == 0 && prefix_[0] == '\0') {
    clearLineBuffer();
    return true;
  }

  char staged[BUS_MAX_PAYLOAD_SIZE - 2];
  memset(staged, 0, sizeof(staged));
  size_t out_len = 0;
  out_len += appendPrefix(staged, sizeof(staged));
  if (out_len < sizeof(staged)) {
    const size_t remaining = sizeof(staged) - 1 - out_len;
    const size_t copy_len = (line_length_ < remaining) ? line_length_ : remaining;
    if (copy_len > 0) {
      memcpy(&staged[out_len], line_buffer_, copy_len);
      out_len += copy_len;
      staged[out_len] = '\0';
    }
    if (copy_len < line_length_) truncated_ = true;
  }

  bus_debug_text_t payload{};
  payload.level = level_;
  payload.text_len = static_cast<uint8_t>(out_len);
  if (out_len > 0) memcpy(payload.text, staged, out_len);

  const bool ok = node_->queueMessage(BUS_MASTER_ADDR, BUS_MSG_DEBUG_TEXT, &payload, static_cast<uint8_t>(2 + out_len));
  if (ok) {
    node_->setDebugPending(true);
    lines_queued_count_++;
  } else {
    lines_dropped_count_++;
  }

  clearLineBuffer();
  return ok;
}

size_t BusDebug::appendPrefix(char* out, size_t out_size) const {
  if (out == nullptr || out_size == 0 || prefix_[0] == '\0') return 0;
  const size_t prefix_len = bounded_strlen(prefix_, out_size - 1);
  if (prefix_len > 0) memcpy(out, prefix_, prefix_len);
  out[prefix_len] = '\0';
  return prefix_len;
}

size_t BusDebug::printNumberUnsigned(unsigned long value) {
  char temp[24];
  const int written = snprintf(temp, sizeof(temp), "%lu", value);
  if (written <= 0) return 0;
  const size_t len = (static_cast<size_t>(written) < sizeof(temp)) ? static_cast<size_t>(written) : (sizeof(temp) - 1);
  return write(reinterpret_cast<const uint8_t*>(temp), len);
}

size_t BusDebug::printNumberSigned(long value) {
  char temp[24];
  const int written = snprintf(temp, sizeof(temp), "%ld", value);
  if (written <= 0) return 0;
  const size_t len = (static_cast<size_t>(written) < sizeof(temp)) ? static_cast<size_t>(written) : (sizeof(temp) - 1);
  return write(reinterpret_cast<const uint8_t*>(temp), len);
}
