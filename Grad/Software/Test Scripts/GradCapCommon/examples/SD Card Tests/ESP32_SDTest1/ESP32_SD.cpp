/*
 * File: HatSD.cpp
 * Description: Implementation of the HatSD microSD helper class. Manages SPI mux selection, filesystem access, test-pattern verification, and throughput measurements.
 * Function count: 14 class methods plus 3 static helper functions and 1 global instance
 * Target microcontroller: ESP32
 */

#include "ESP32_SD.h"
#include <stdarg.h>
#include <stdlib.h>

HatSDClass HatSD;

HatSDClass::HatSDClass()
    : _spi(nullptr),
      _log(nullptr),
      _mounted(false),
      _mountFreqHz(4000000) {}

void HatSDClass::logf(const char* fmt, ...) {
  if (!_log) return;

  char buf[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  _log->print(buf);
}

void HatSDClass::selectSD() {
  hat_spi_mux_select_sd();
  delayMicroseconds(5);
}

void HatSDClass::deselectSD() {
  hat_spi_mux_disconnect();
  delayMicroseconds(5);
}

bool HatSDClass::begin(Print* log, SPIClass& spi, uint32_t mount_freq_hz) {
  _log = log;
  _spi = &spi;
  _mountFreqHz = mount_freq_hz;

  logf("[HatSD] Starting SD bring-up...\r\n");

  hat_spi_mux_begin();
  hat_spi_mux_disconnect();

  // Initialize SPI bus with your board pinout.
  _spi->begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

  // Dummy CS only exists to satisfy SD.begin().
  pinMode(DUMMY_CS_PIN, OUTPUT);
  digitalWrite(DUMMY_CS_PIN, HIGH);

  // Select SD path through mux before mounting.
  selectSD();

  if (!SD.begin(DUMMY_CS_PIN, *_spi, _mountFreqHz)) {
    logf("[HatSD] ERROR: SD.begin() failed.\r\n");
    deselectSD();
    _mounted = false;
    return false;
  }

  _mounted = true;
  logf("[HatSD] SD.begin() OK.\r\n");

  // Quick sanity check: root should open if filesystem is valid/readable.
  File root = SD.open("/");
  if (!root) {
    logf("[HatSD] ERROR: mounted device but could not open root directory.\r\n");
    SD.end();
    deselectSD();
    _mounted = false;
    return false;
  }

  if (!root.isDirectory()) {
    logf("[HatSD] ERROR: root opened but is not a directory.\r\n");
    root.close();
    SD.end();
    deselectSD();
    _mounted = false;
    return false;
  }

  root.close();
  logf("[HatSD] Root directory opened successfully.\r\n");
  logf("[HatSD] Filesystem appears readable.\r\n");
  logf("[HatSD] NOTE: this verifies a supported mounted filesystem. "
       "It does not explicitly distinguish FAT16 vs FAT32.\r\n");

  deselectSD();
  return true;
}

void HatSDClass::end() {
  if (_mounted) {
    selectSD();
    SD.end();
    deselectSD();
  }
  _mounted = false;
}

bool HatSDClass::mounted() const {
  return _mounted;
}

uint32_t HatSDClass::mountFrequencyHz() const {
  return _mountFreqHz;
}

bool HatSDClass::printCardInfo(Print& out) {
  if (!_mounted) {
    out.println("[HatSD] ERROR: card not mounted.");
    return false;
  }

  selectSD();

  sdcard_type_t type = SD.cardType();
  uint64_t cardSizeMB = SD.cardSize() / (1024ULL * 1024ULL);
  uint64_t totalMB    = SD.totalBytes() / (1024ULL * 1024ULL);
  uint64_t usedMB     = SD.usedBytes() / (1024ULL * 1024ULL);

  out.println("[HatSD] Card information:");
  out.print("  Card type: ");
  switch (type) {
    case CARD_MMC:  out.println("MMC"); break;
    case CARD_SD:   out.println("SDSC"); break;
    case CARD_SDHC: out.println("SDHC/SDXC"); break;
    case CARD_NONE: out.println("NONE"); break;
    default:        out.println("UNKNOWN"); break;
  }

  out.print("  Raw card capacity: ");
  out.print((unsigned long)cardSizeMB);
  out.println(" MB");

  out.print("  Mounted filesystem total: ");
  out.print((unsigned long)totalMB);
  out.println(" MB");

  out.print("  Mounted filesystem used: ");
  out.print((unsigned long)usedMB);
  out.println(" MB");

  deselectSD();
  return true;
}

bool HatSDClass::exists(const char* path) {
  if (!_mounted) return false;

  selectSD();
  bool ok = SD.exists(path);
  deselectSD();
  return ok;
}

bool HatSDClass::removeFile(const char* path) {
  if (!_mounted) return false;

  selectSD();
  bool ok = true;
  if (SD.exists(path)) {
    ok = SD.remove(path);
  }
  deselectSD();
  return ok;
}

bool HatSDClass::listDirRecursive(Print& out, const char* path, uint8_t levels, uint8_t depth) {
  File dir = SD.open(path);
  if (!dir) {
    out.print("[HatSD] ERROR: failed to open directory: ");
    out.println(path);
    return false;
  }

  if (!dir.isDirectory()) {
    out.print("[HatSD] ERROR: not a directory: ");
    out.println(path);
    dir.close();
    return false;
  }

  File entry = dir.openNextFile();
  while (entry) {
    for (uint8_t i = 0; i < depth; i++) {
      out.print("  ");
    }

    if (entry.isDirectory()) {
      out.print("[DIR]  ");
      out.println(entry.name());

      if (levels > 0) {
        String childPath = String(path);
        if (!childPath.endsWith("/")) childPath += "/";
        childPath += entry.name();

        entry.close();
        listDirRecursive(out, childPath.c_str(), levels - 1, depth + 1);

        entry = dir.openNextFile();
        continue;
      }
    } else {
      out.print("[FILE] ");
      out.print(entry.name());
      out.print("  (");
      out.print((unsigned long)entry.size());
      out.println(" bytes)");
    }

    entry.close();
    entry = dir.openNextFile();
  }

  dir.close();
  return true;
}

bool HatSDClass::listDir(Print& out, const char* path, uint8_t levels) {
  if (!_mounted) {
    out.println("[HatSD] ERROR: card not mounted.");
    return false;
  }

  selectSD();
  bool ok = listDirRecursive(out, path, levels, 0);
  deselectSD();
  return ok;
}

bool HatSDClass::printFilePreview(Print& out, const char* path, size_t max_bytes) {
  if (!_mounted) {
    out.println("[HatSD] ERROR: card not mounted.");
    return false;
  }

  selectSD();

  File file = SD.open(path, FILE_READ);
  if (!file) {
    out.print("[HatSD] ERROR: failed to open file: ");
    out.println(path);
    deselectSD();
    return false;
  }

  out.print("[HatSD] Preview of ");
  out.print(path);
  out.print(" (up to ");
  out.print((unsigned long)max_bytes);
  out.println(" bytes):");

  size_t count = 0;
  while (file.available() && count < max_bytes) {
    int c = file.read();
    if (c < 0) break;

    if (c >= 32 && c <= 126) {
      out.write((char)c);
    } else if (c == '\r' || c == '\n' || c == '\t') {
      out.write((char)c);
    } else {
      out.print('.');
    }

    count++;
  }

  out.println();
  out.print("[HatSD] Bytes shown: ");
  out.println((unsigned long)count);

  file.close();
  deselectSD();
  return true;
}

void HatSDClass::fillTestPattern(uint8_t* buf, size_t len, uint32_t base_offset) {
  for (size_t i = 0; i < len; i++) {
    uint32_t x = base_offset + (uint32_t)i;
    buf[i] = (uint8_t)((x ^ (x >> 8) ^ 0xA5U) & 0xFFU);
  }
}

bool HatSDClass::verifyTestPattern(const uint8_t* buf, size_t len, uint32_t base_offset) {
  for (size_t i = 0; i < len; i++) {
    uint32_t x = base_offset + (uint32_t)i;
    uint8_t expected = (uint8_t)((x ^ (x >> 8) ^ 0xA5U) & 0xFFU);
    if (buf[i] != expected) {
      return false;
    }
  }
  return true;
}

float HatSDClass::calcKBps(uint32_t bytes, uint32_t elapsed_ms) {
  if (elapsed_ms == 0) return 0.0f;
  return ((float)bytes * 1000.0f) / ((float)elapsed_ms * 1024.0f);
}

bool HatSDClass::writeReadVerifyTest(Print& out,
                                     const char* path,
                                     size_t total_bytes,
                                     size_t chunk_size,
                                     bool delete_after) {
  HatSDThroughputResult result;
  bool ok = throughputTest(out, path, result, total_bytes, chunk_size, delete_after);

  out.println("[HatSD] Functional write/read/verify result:");
  out.print("  mount_ok: ");
  out.println(result.mount_ok ? "true" : "false");
  out.print("  write_ok: ");
  out.println(result.write_ok ? "true" : "false");
  out.print("  read_ok: ");
  out.println(result.read_ok ? "true" : "false");
  out.print("  verify_ok: ");
  out.println(result.verify_ok ? "true" : "false");
  out.print("  cleanup_ok: ");
  out.println(result.cleanup_ok ? "true" : "false");

  return ok;
}

bool HatSDClass::throughputTest(Print& out,
                                const char* path,
                                HatSDThroughputResult& result,
                                size_t total_bytes,
                                size_t chunk_size,
                                bool delete_after) {
  result = HatSDThroughputResult{};
  result.spi_hz = _mountFreqHz;
  result.total_bytes = (uint32_t)total_bytes;
  result.chunk_size = chunk_size;
  result.mount_ok = _mounted;

  if (!_mounted) {
    out.println("[HatSD] ERROR: card not mounted.");
    return false;
  }

  if (chunk_size == 0 || total_bytes == 0) {
    out.println("[HatSD] ERROR: invalid throughput test sizes.");
    return false;
  }

  uint8_t* txbuf = (uint8_t*)malloc(chunk_size);
  uint8_t* rxbuf = (uint8_t*)malloc(chunk_size);

  if (!txbuf || !rxbuf) {
    out.println("[HatSD] ERROR: failed to allocate test buffers.");
    if (txbuf) free(txbuf);
    if (rxbuf) free(rxbuf);
    return false;
  }

  // Remove any stale test file first.
  removeFile(path);

  out.println("[HatSD] ----------------------------------");
  out.print("[HatSD] Throughput test file: ");
  out.println(path);
  out.print("[HatSD] SPI frequency: ");
  out.print(_mountFreqHz);
  out.print(" Hz (");
  out.print(_mountFreqHz / 1000000.0, 2);
  out.println(" MHz)");
  out.print("[HatSD] Total bytes: ");
  out.println((unsigned long)total_bytes);
  out.print("[HatSD] Chunk size: ");
  out.println((unsigned long)chunk_size);

  bool success = true;

  // --------------------
  // Write pass
  // --------------------
  selectSD();
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    out.println("[HatSD] ERROR: failed to open test file for write.");
    deselectSD();
    free(txbuf);
    free(rxbuf);
    return false;
  }

  uint32_t write_start = millis();
  size_t written_total = 0;

  while (written_total < total_bytes) {
    size_t this_chunk = chunk_size;
    if (this_chunk > (total_bytes - written_total)) {
      this_chunk = total_bytes - written_total;
    }

    fillTestPattern(txbuf, this_chunk, (uint32_t)written_total);

    size_t w = file.write(txbuf, this_chunk);
    if (w != this_chunk) {
      out.print("[HatSD] ERROR: short write at offset ");
      out.println((unsigned long)written_total);
      result.write_ok = false;
      success = false;
      break;
    }

    written_total += w;
  }

  file.flush();
  file.close();
  uint32_t write_end = millis();
  deselectSD();

  result.write_time_ms = write_end - write_start;
  result.write_ok = success && (written_total == total_bytes);
  result.write_kBps = calcKBps((uint32_t)written_total, result.write_time_ms);

  out.print("[HatSD] Write bytes: ");
  out.println((unsigned long)written_total);
  out.print("[HatSD] Write time: ");
  out.print((unsigned long)result.write_time_ms);
  out.println(" ms");
  out.print("[HatSD] Write speed: ");
  out.print(result.write_kBps, 2);
  out.println(" kB/s");

  if (!result.write_ok) {
    removeFile(path);
    free(txbuf);
    free(rxbuf);
    return false;
  }

  // --------------------
  // Read + verify pass
  // --------------------
  selectSD();
  file = SD.open(path, FILE_READ);
  if (!file) {
    out.println("[HatSD] ERROR: failed to open test file for read.");
    deselectSD();
    removeFile(path);
    free(txbuf);
    free(rxbuf);
    return false;
  }

  uint32_t read_start = millis();
  size_t read_total = 0;
  bool verify_ok = true;

  while (read_total < total_bytes) {
    size_t this_chunk = chunk_size;
    if (this_chunk > (total_bytes - read_total)) {
      this_chunk = total_bytes - read_total;
    }

    int r = file.read(rxbuf, this_chunk);
    if (r != (int)this_chunk) {
      out.print("[HatSD] ERROR: short read at offset ");
      out.println((unsigned long)read_total);
      verify_ok = false;
      success = false;
      break;
    }

    if (!verifyTestPattern(rxbuf, this_chunk, (uint32_t)read_total)) {
      out.print("[HatSD] ERROR: data verify failed at offset ");
      out.println((unsigned long)read_total);
      verify_ok = false;
      success = false;
      break;
    }

    read_total += (size_t)r;
  }

  file.close();
  uint32_t read_end = millis();
  deselectSD();

  result.read_time_ms = read_end - read_start;
  result.read_ok = (read_total == total_bytes);
  result.verify_ok = verify_ok;
  result.read_kBps = calcKBps((uint32_t)read_total, result.read_time_ms);

  out.print("[HatSD] Read bytes: ");
  out.println((unsigned long)read_total);
  out.print("[HatSD] Read time: ");
  out.print((unsigned long)result.read_time_ms);
  out.println(" ms");
  out.print("[HatSD] Read speed: ");
  out.print(result.read_kBps, 2);
  out.println(" kB/s");

  // --------------------
  // Cleanup
  // --------------------
  if (delete_after) {
    result.cleanup_ok = removeFile(path);
    out.print("[HatSD] Cleanup delete: ");
    out.println(result.cleanup_ok ? "OK" : "FAILED");
  } else {
    result.cleanup_ok = true;
    out.println("[HatSD] Cleanup delete: skipped");
  }

  free(txbuf);
  free(rxbuf);

  return result.write_ok && result.read_ok && result.verify_ok;
}

bool HatSDClass::throughputSweep(Print& out,
                                 const uint32_t* freqs_hz,
                                 size_t freq_count,
                                 size_t total_bytes,
                                 size_t chunk_size,
                                 bool delete_after) {
  if (!freqs_hz || freq_count == 0) {
    out.println("[HatSD] ERROR: no frequencies provided for sweep.");
    return false;
  }

  if (!_spi) {
    out.println("[HatSD] ERROR: SPI object not initialized.");
    return false;
  }

  bool any_success = false;

  out.println("[HatSD] ==================================");
  out.println("[HatSD] Starting SD throughput sweep");
  out.println("[HatSD] ==================================");

  for (size_t i = 0; i < freq_count; i++) {
    uint32_t hz = freqs_hz[i];

    out.println();
    out.println("[HatSD] ----------------------------------");
    out.print("[HatSD] Sweep point ");
    out.print((unsigned long)(i + 1));
    out.print("/");
    out.println((unsigned long)freq_count);
    out.print("[HatSD] Requested SPI frequency: ");
    out.print((unsigned long)hz);
    out.println(" Hz");

    end();

    if (!begin(_log, *_spi, hz)) {
      out.println("[HatSD] Mount failed at this frequency.");
      continue;
    }

    String path = "/SDSPT_";
    path += String((unsigned long)hz);
    path += ".BIN";

    HatSDThroughputResult result;
    bool ok = throughputTest(out, path.c_str(), result, total_bytes, chunk_size, delete_after);

    out.print("[HatSD] Sweep result @ ");
    out.print((unsigned long)hz);
    out.print(" Hz: ");
    out.println(ok ? "PASS" : "FAIL");

    if (ok) {
      any_success = true;
    }
  }

  out.println();
  out.println("[HatSD] Throughput sweep complete.");
  return any_success;
}
