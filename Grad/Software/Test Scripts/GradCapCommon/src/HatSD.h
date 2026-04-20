/*
 * File: HatSD.h
 * Description: C++ helper class for accessing the microSD card through the SPI mux. Provides mount control, directory/file inspection, functional tests, and throughput benchmarks.
 * Function count: 14 public methods, 5 private helpers, and 1 result structure
 * Target microcontroller: ESP32
 */

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
  /* Construct an unmounted HatSD helper object.
     Input: None.
     Output: New HatSDClass object. */
  HatSDClass();

  // Mount card and verify the filesystem is readable.
  /* Mount the microSD card through the SPI mux and verify basic filesystem access.
     Input: Optional log stream, SPI object, and mount frequency in hertz.
     Output: true on success, false on failure. */
  bool begin(Print* log = nullptr,
             SPIClass& spi = SPI,
             uint32_t mount_freq_hz = 4000000);

  /* Unmount the microSD card and disconnect it from the shared SPI bus.
     Input: None.
     Output: None. */
  void end();
  /* Report whether the microSD card is currently mounted.
     Input: None.
     Output: true if mounted, otherwise false. */
  bool mounted() const;
  /* Return the SPI mount frequency currently associated with the mounted card.
     Input: None.
     Output: Frequency in hertz. */
  uint32_t mountFrequencyHz() const;

  // Information helpers
  /* Print basic card capacity and usage information to a log stream.
     Input: Destination Print stream.
     Output: true on success, false on failure. */
  bool printCardInfo(Print& out);
  /* Recursively print directory contents from a starting path.
     Input: Destination stream, root path, and recursion depth.
     Output: true on success, false on failure. */
  bool listDir(Print& out, const char* path = "/", uint8_t levels = 1);
  /* Print the first part of a file for quick inspection.
     Input: Destination stream, file path, and maximum number of bytes to preview.
     Output: true on success, false on failure. */
  bool printFilePreview(Print& out, const char* path, size_t max_bytes = 512);
  /* Check whether a file or directory exists on the mounted card.
     Input: Path string.
     Output: true if present, otherwise false. */
  bool exists(const char* path);
  /* Remove a file if it exists.
     Input: Path string.
     Output: true on success, false on failure. */
  bool removeFile(const char* path);

  // Controlled functional test: write, read back, verify, optionally delete.
  /* Perform a controlled file write, read-back, and verification test.
     Input: Destination stream, file path, total byte count, chunk size, and optional delete flag.
     Output: true if the test passes, otherwise false. */
  bool writeReadVerifyTest(Print& out,
                           const char* path,
                           size_t total_bytes = 4096,
                           size_t chunk_size = 512,
                           bool delete_after = true);

  // Throughput test at the CURRENT mounted SPI frequency.
  /* Measure write and read throughput at the currently mounted SPI frequency.
     Input: Destination stream, file path, result structure, total byte count, chunk size, and optional delete flag.
     Output: true if the test completes, otherwise false. */
  bool throughputTest(Print& out,
                      const char* path,
                      HatSDThroughputResult& result,
                      size_t total_bytes = 262144,
                      size_t chunk_size = 4096,
                      bool delete_after = true);

  // Sweep across multiple mount frequencies. The card is remounted at each speed.
  /* Re-mount and test the card across multiple SPI frequencies.
     Input: Destination stream, list of frequencies, count, total byte count, chunk size, and optional delete flag.
     Output: true if the sweep completes, otherwise false. */
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

  /* Write a formatted diagnostic line to the optional log stream.
     Input: printf-style format string and arguments.
     Output: None. */
  void logf(const char* fmt, ...);
  /* Connect the microSD path to the shared SPI bus.
     Input: None.
     Output: None. */
  void selectSD();
  /* Disconnect the microSD path from the shared SPI bus.
     Input: None.
     Output: None. */
  void deselectSD();

  /* Internal recursive helper used by listDir().
     Input: Destination stream, current path, remaining recursion levels, and current indentation depth.
     Output: true on success, false on failure. */
  bool listDirRecursive(Print& out, const char* path, uint8_t levels, uint8_t depth);

  /* Fill a buffer with a deterministic byte pattern for SD test operations.
     Input: Buffer pointer, length, and base offset/seed.
     Output: None. */
  static void fillTestPattern(uint8_t* buf, size_t len, uint32_t base_offset);
  /* Verify that a buffer matches the deterministic SD test pattern.
     Input: Buffer pointer, length, and base offset/seed.
     Output: true if all bytes match, otherwise false. */
  static bool verifyTestPattern(const uint8_t* buf, size_t len, uint32_t base_offset);
  /* Convert byte count and elapsed milliseconds into kB/s.
     Input: Byte count and elapsed time in milliseconds.
     Output: Throughput in kB/s. */
  static float calcKBps(uint32_t bytes, uint32_t elapsed_ms);
};

extern HatSDClass HatSD;
