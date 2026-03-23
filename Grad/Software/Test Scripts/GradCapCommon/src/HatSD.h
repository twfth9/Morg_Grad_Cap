#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

#include "HatPins.h"
#include "HatSpiMux.h"

struct HatSDThroughputResult {
  uint32_t spi_hz = 0;
  uint32_t total_bytes = 0;
  size_t chunk_size = 0;

  bool mount_ok = false;
  bool write_ok = false;
  bool read_ok = false;
  bool verify_ok = false;
  bool cleanup_ok = false;

  uint32_t write_time_ms = 0;
  uint32_t read_time_ms = 0;

  float write_kBps = 0.0f;
  float read_kBps = 0.0f;
};

class HatSDClass {
public:
  HatSDClass();

  // Mount card and verify the filesystem is readable.
  bool begin(Print* log = nullptr,
             SPIClass& spi = SPI,
             uint32_t mount_freq_hz = 4000000);

  void end();
  bool mounted() const;
  uint32_t mountFrequencyHz() const;

  // Information helpers
  bool printCardInfo(Print& out);
  bool listDir(Print& out, const char* path = "/", uint8_t levels = 1);
  bool printFilePreview(Print& out, const char* path, size_t max_bytes = 512);
  bool exists(const char* path);
  bool removeFile(const char* path);

  // Controlled functional test: write, read back, verify, optionally delete.
  bool writeReadVerifyTest(Print& out,
                           const char* path,
                           size_t total_bytes = 4096,
                           size_t chunk_size = 512,
                           bool delete_after = true);

  // Throughput test at the CURRENT mounted SPI frequency.
  bool throughputTest(Print& out,
                      const char* path,
                      HatSDThroughputResult& result,
                      size_t total_bytes = 262144,
                      size_t chunk_size = 4096,
                      bool delete_after = true);

  // Sweep across multiple mount frequencies. The card is remounted at each speed.
  bool throughputSweep(Print& out,
                       const uint32_t* freqs_hz,
                       size_t freq_count,
                       size_t total_bytes = 262144,
                       size_t chunk_size = 4096,
                       bool delete_after = true);

private:
  SPIClass* _spi;
  Print* _log;
  bool _mounted;
  uint32_t _mountFreqHz;

  // Dummy CS pin used only to satisfy SD.begin().
  // The real SD chip-select is handled by the SPI mux hardware.
  static constexpr int DUMMY_CS_PIN = PIN_STRAP_3_UNUSED;

  void logf(const char* fmt, ...);
  void selectSD();
  void deselectSD();

  bool listDirRecursive(Print& out, const char* path, uint8_t levels, uint8_t depth);

  static void fillTestPattern(uint8_t* buf, size_t len, uint32_t base_offset);
  static bool verifyTestPattern(const uint8_t* buf, size_t len, uint32_t base_offset);
  static float calcKBps(uint32_t bytes, uint32_t elapsed_ms);
};

extern HatSDClass HatSD;