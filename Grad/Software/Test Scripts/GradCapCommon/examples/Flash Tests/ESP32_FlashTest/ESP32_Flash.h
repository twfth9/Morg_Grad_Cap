/*
 * File: HatFlash.h
 * Description: C-style driver API for the external SPI flash device on the hat board. Provides bring-up, ID reads, erase/program/read operations, verification helpers, and benchmark helpers.
 * Function count: 22 public API functions and 3 public data structures
 * Target microcontroller: ESP32
 */

#pragma once
#include <Arduino.h>
#include <SPI.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t jedec_id;
  uint8_t manufacturer_id;
  uint8_t memory_type;
  uint8_t capacity_code;
  uint32_t capacity_bytes;
  uint8_t unique_id[8];
  bool unique_id_valid;
} hat_flash_info_t;

typedef struct {
  uint32_t spi_hz;
  bool passed;
  bool used_fast_read;
  float read_kib_per_sec;
  float write_kib_per_sec;
} hat_flash_speed_point_t;

typedef struct {
  uint32_t max_pass_hz;
  uint32_t first_fail_hz;
  bool found_failure;
  size_t points_tested;
  hat_flash_speed_point_t points[16];
} hat_flash_speed_sweep_result_t;

// Bring-up / configuration
/* Initialize the external flash driver and shared SPI bus settings.
   Input: Desired SPI clock frequency in hertz.
   Output: true on success, false on failure. */
bool     hat_flash_begin(uint32_t spi_hz);
/* Shut down the flash driver and leave the SPI target disconnected.
   Input: None.
   Output: None. */
void     hat_flash_end(void);
/* Report whether the flash driver has been initialized.
   Input: None.
   Output: true if initialized, otherwise false. */
bool     hat_flash_is_started(void);
/* Update the SPI clock frequency used for future flash transactions.
   Input: Desired SPI clock frequency in hertz.
   Output: None. */
void     hat_flash_set_spi_hz(uint32_t spi_hz);
/* Return the SPI clock frequency currently configured for flash access.
   Input: None.
   Output: SPI clock in hertz. */
uint32_t hat_flash_get_spi_hz(void);

/* Issue the release-from-power-down command to the flash chip.
   Input: None.
   Output: true on success, false on failure. */
bool     hat_flash_release_powerdown(void);
/* Issue the software reset sequence to the flash chip.
   Input: None.
   Output: true on success, false on failure. */
bool     hat_flash_software_reset(void);

// ID / info
/* Read the 24-bit JEDEC identifier from the flash chip.
   Input: None.
   Output: JEDEC ID value. */
uint32_t hat_flash_read_jedec_id(void);
/* Read the 64-bit factory unique ID from the flash chip.
   Input: Output buffer for 8 bytes.
   Output: true on success, false on failure. */
bool     hat_flash_read_unique_id(uint8_t out[8]);
/* Return the flash capacity in bytes based on the detected JEDEC ID.
   Input: None.
   Output: Capacity in bytes, or 0 if unknown. */
uint32_t hat_flash_size_bytes(void);
/* Fill a caller-supplied info structure with decoded flash identification data.
   Input: Pointer to destination info structure.
   Output: true on success, false on failure. */
bool     hat_flash_get_info(hat_flash_info_t* out);

// Accept either exact IDs or the common Winbond 128Mbit variants.
/* Compare the detected JEDEC fields against expected values.
   Input: Expected manufacturer, memory-type, and capacity-code fields.
   Output: true if they match, otherwise false. */
bool     hat_flash_validate_expected_jedec(uint8_t manufacturer,
                                           uint8_t mem_type,
                                           uint8_t capacity_code);
/* Check whether the detected flash looks like a Winbond W25Q128JV-family device.
   Input: None.
   Output: true if the ID matches an expected variant. */
bool     hat_flash_is_w25q128jv(void);

// Status / control
/* Read status register 1 from the flash chip.
   Input: None.
   Output: Register byte value. */
uint8_t  hat_flash_read_status1(void);
/* Read status register 2 from the flash chip.
   Input: None.
   Output: Register byte value. */
uint8_t  hat_flash_read_status2(void);
/* Read status register 3 from the flash chip.
   Input: None.
   Output: Register byte value. */
uint8_t  hat_flash_read_status3(void);
/* Set the write-enable latch before an erase or program command.
   Input: None.
   Output: true on success, false on failure. */
bool     hat_flash_write_enable(void);
/* Clear the write-enable latch.
   Input: None.
   Output: true on success, false on failure. */
bool     hat_flash_write_disable(void);
/* Poll the flash busy flag until the device is idle or a timeout expires.
   Input: Timeout in milliseconds.
   Output: true if ready before timeout, otherwise false. */
bool     hat_flash_wait_while_busy(uint32_t timeout_ms);

// Read / write / erase
/* Read bytes from flash using the standard READ (03h) command.
   Input: 24-bit address, destination buffer, and byte count.
   Output: true on success, false on failure. */
bool     hat_flash_read(uint32_t addr, uint8_t* data, size_t len);          // 03h
/* Read bytes from flash using the FAST READ (0Bh) command.
   Input: 24-bit address, destination buffer, and byte count.
   Output: true on success, false on failure. */
bool     hat_flash_fast_read(uint32_t addr, uint8_t* data, size_t len);     // 0Bh
/* Read bytes from flash using either normal or fast read based on the configured SPI clock.
   Input: 24-bit address, destination buffer, and byte count.
   Output: true on success, false on failure. */
bool     hat_flash_read_auto(uint32_t addr, uint8_t* data, size_t len);     // chooses based on SPI hz

/* Erase the 4 kB sector containing the supplied address.
   Input: Address inside the target sector.
   Output: true on success, false on failure. */
bool     hat_flash_sector_erase_4k(uint32_t addr);
/* Erase the 32 kB block containing the supplied address.
   Input: Address inside the target block.
   Output: true on success, false on failure. */
bool     hat_flash_block_erase_32k(uint32_t addr);
/* Erase the 64 kB block containing the supplied address.
   Input: Address inside the target block.
   Output: true on success, false on failure. */
bool     hat_flash_block_erase_64k(uint32_t addr);
/* Erase the entire flash device.
   Input: None.
   Output: true on success, false on failure. */
bool     hat_flash_chip_erase(void);

/* Program up to one page of data starting at the supplied address.
   Input: Start address, source buffer, and byte count.
   Output: true on success, false on failure. */
bool     hat_flash_page_program(uint32_t addr, const uint8_t* data, size_t len);
/* Write an arbitrary-length data block by splitting it into one or more page-program operations.
   Input: Start address, source buffer, and byte count.
   Output: true on success, false on failure. */
bool     hat_flash_write(uint32_t addr, const uint8_t* data, size_t len);

// Verification helpers
/* Write and verify a generated test pattern at the supplied address range.
   Input: Start address and test length in bytes.
   Output: true if the test passes, otherwise false. */
bool     hat_flash_test_pattern(uint32_t addr, size_t len);
/* Read back flash contents and compare them against caller-supplied expected data.
   Input: Start address, expected-data buffer, and byte count.
   Output: true if all bytes match, otherwise false. */
bool     hat_flash_verify_data(uint32_t addr, const uint8_t* expected, size_t len);

// Speed / benchmark helpers
/* Measure sustained flash read throughput over the supplied test range.
   Input: Start address, test length, chunk size, and output pointer for KiB/s.
   Output: true on success, false on failure. */
bool     hat_flash_measure_read_speed(uint32_t addr,
                                      size_t len,
                                      size_t chunk_size,
                                      float* kib_per_sec);

/* Measure sustained flash write throughput over the supplied test range.
   Input: Start address, test length, chunk size, and output pointer for KiB/s.
   Output: true on success, false on failure. */
bool     hat_flash_measure_write_speed(uint32_t addr,
                                       size_t len,
                                       size_t chunk_size,
                                       float* kib_per_sec);

/* Sweep across multiple SPI clock frequencies and record pass/fail and throughput results.
   Input: Test address, test length, frequency list, frequency count, and destination result structure.
   Output: true on success, false on failure. */
bool     hat_flash_speed_sweep(uint32_t test_addr,
                               size_t test_len,
                               const uint32_t* speeds_hz,
                               size_t speed_count,
                               hat_flash_speed_sweep_result_t* out);

#ifdef __cplusplus
}
#endif
