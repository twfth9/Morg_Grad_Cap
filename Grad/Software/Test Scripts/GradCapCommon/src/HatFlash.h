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
bool     hat_flash_begin(uint32_t spi_hz);
void     hat_flash_end(void);
bool     hat_flash_is_started(void);
void     hat_flash_set_spi_hz(uint32_t spi_hz);
uint32_t hat_flash_get_spi_hz(void);

bool     hat_flash_release_powerdown(void);
bool     hat_flash_software_reset(void);

// ID / info
uint32_t hat_flash_read_jedec_id(void);
bool     hat_flash_read_unique_id(uint8_t out[8]);
uint32_t hat_flash_size_bytes(void);
bool     hat_flash_get_info(hat_flash_info_t* out);

// Accept either exact IDs or the common Winbond 128Mbit variants.
bool     hat_flash_validate_expected_jedec(uint8_t manufacturer,
                                           uint8_t mem_type,
                                           uint8_t capacity_code);
bool     hat_flash_is_w25q128jv(void);

// Status / control
uint8_t  hat_flash_read_status1(void);
uint8_t  hat_flash_read_status2(void);
uint8_t  hat_flash_read_status3(void);
bool     hat_flash_write_enable(void);
bool     hat_flash_write_disable(void);
bool     hat_flash_wait_while_busy(uint32_t timeout_ms);

// Read / write / erase
bool     hat_flash_read(uint32_t addr, uint8_t* data, size_t len);          // 03h
bool     hat_flash_fast_read(uint32_t addr, uint8_t* data, size_t len);     // 0Bh
bool     hat_flash_read_auto(uint32_t addr, uint8_t* data, size_t len);     // chooses based on SPI hz

bool     hat_flash_sector_erase_4k(uint32_t addr);
bool     hat_flash_block_erase_32k(uint32_t addr);
bool     hat_flash_block_erase_64k(uint32_t addr);
bool     hat_flash_chip_erase(void);

bool     hat_flash_page_program(uint32_t addr, const uint8_t* data, size_t len);
bool     hat_flash_write(uint32_t addr, const uint8_t* data, size_t len);

// Verification helpers
bool     hat_flash_test_pattern(uint32_t addr, size_t len);
bool     hat_flash_verify_data(uint32_t addr, const uint8_t* expected, size_t len);

// Speed / benchmark helpers
bool     hat_flash_measure_read_speed(uint32_t addr,
                                      size_t len,
                                      size_t chunk_size,
                                      float* kib_per_sec);

bool     hat_flash_measure_write_speed(uint32_t addr,
                                       size_t len,
                                       size_t chunk_size,
                                       float* kib_per_sec);

bool     hat_flash_speed_sweep(uint32_t test_addr,
                               size_t test_len,
                               const uint32_t* speeds_hz,
                               size_t speed_count,
                               hat_flash_speed_sweep_result_t* out);

#ifdef __cplusplus
}
#endif