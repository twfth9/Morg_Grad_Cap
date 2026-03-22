#pragma once
#include <Arduino.h>
#include <SPI.h>

#ifdef __cplusplus
extern "C" {
#endif

bool     hat_flash_begin(uint32_t spi_hz);
void     hat_flash_end(void);

uint32_t hat_flash_read_jedec_id(void);
bool     hat_flash_read_unique_id(uint8_t out[8]);

uint32_t hat_flash_size_bytes(void);
bool     hat_flash_validate_expected_jedec(uint8_t manufacturer,
                                           uint8_t mem_type,
                                           uint8_t capacity_code);

bool     hat_flash_read(uint32_t addr, uint8_t* data, size_t len);
bool     hat_flash_write_enable(void);
bool     hat_flash_wait_while_busy(uint32_t timeout_ms);
uint8_t  hat_flash_read_status1(void);
uint8_t  hat_flash_read_status2(void);
uint8_t  hat_flash_read_status3(void);

bool     hat_flash_sector_erase_4k(uint32_t addr);
bool     hat_flash_block_erase_32k(uint32_t addr);
bool     hat_flash_block_erase_64k(uint32_t addr);
bool     hat_flash_chip_erase(void);

bool     hat_flash_page_program(uint32_t addr, const uint8_t* data, size_t len);
bool     hat_flash_write(uint32_t addr, const uint8_t* data, size_t len);

bool     hat_flash_test_pattern(uint32_t addr, size_t len);
bool     hat_flash_measure_read_speed(uint32_t addr,
                                      size_t len,
                                      size_t chunk_size,
                                      float* kb_per_sec);
bool     hat_flash_measure_write_speed(uint32_t addr,
                                       size_t len,
                                       size_t chunk_size,
                                       float* kb_per_sec);

#ifdef __cplusplus
}
#endif