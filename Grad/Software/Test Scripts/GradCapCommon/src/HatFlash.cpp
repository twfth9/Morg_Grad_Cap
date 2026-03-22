#include "HatFlash.h"

#include "HatPins.h"
#include "HatSpiMux.h"

static SPIClass* g_spi = &SPI;
static bool g_started = false;
static uint32_t g_spi_hz = 1000000;

// Winbond W25Q128JV command set
#define CMD_WRITE_ENABLE         0x06
#define CMD_WRITE_DISABLE        0x04
#define CMD_READ_STATUS1         0x05
#define CMD_READ_STATUS2         0x35
#define CMD_READ_STATUS3         0x15
#define CMD_READ_DATA            0x03
#define CMD_FAST_READ            0x0B
#define CMD_PAGE_PROGRAM         0x02
#define CMD_SECTOR_ERASE_4K      0x20
#define CMD_BLOCK_ERASE_32K      0x52
#define CMD_BLOCK_ERASE_64K      0xD8
#define CMD_CHIP_ERASE_C7        0xC7
#define CMD_JEDEC_ID             0x9F
#define CMD_UNIQUE_ID            0x4B
#define CMD_RELEASE_POWERDOWN    0xAB
#define CMD_ENABLE_RESET         0x66
#define CMD_RESET_DEVICE         0x99

#define STATUS1_BUSY_MASK        0x01
#define STATUS1_WEL_MASK         0x02

#define FLASH_PAGE_SIZE          256UL
#define FLASH_SECTOR_SIZE        4096UL
#define FLASH_STANDARD_READ_MAX_HZ 50000000UL

static inline SPISettings flash_spi_settings(void) {
  return SPISettings(g_spi_hz, MSBFIRST, SPI_MODE0);
}

static inline void flash_select(void) {
  hat_spi_mux_select_flash();
}

static inline void flash_deselect(void) {
  hat_spi_mux_disconnect();
}

static inline void flash_begin_transaction(void) {
  g_spi->beginTransaction(flash_spi_settings());
  flash_select();
}

static inline void flash_end_transaction(void) {
  flash_deselect();
  g_spi->endTransaction();
}

static void flash_write_addr24(uint32_t addr) {
  g_spi->transfer((addr >> 16) & 0xFF);
  g_spi->transfer((addr >> 8) & 0xFF);
  g_spi->transfer(addr & 0xFF);
}

static uint32_t capacity_code_to_bytes(uint8_t code) {
  if (code < 0x10 || code > 0x30) return 0;
  return (1UL << code);
}

static void fill_test_pattern(uint8_t* buf, size_t len, uint32_t seed) {
  if (!buf) return;
  for (size_t i = 0; i < len; ++i) {
    uint32_t x = seed + (uint32_t)i * 37u + ((uint32_t)i << 7);
    buf[i] = (uint8_t)(x ^ (x >> 8) ^ 0x5A);
  }
}

bool hat_flash_begin(uint32_t spi_hz) {
  g_spi_hz = spi_hz;

  pinMode(PIN_SPI_MISO, INPUT_PULLUP);
  g_spi->begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

  flash_deselect();
  delay(2);

  g_started = true;
  return true;
}

void hat_flash_end(void) {
  if (!g_started) return;
  flash_deselect();
  g_spi->end();
  g_started = false;
}

bool hat_flash_is_started(void) {
  return g_started;
}

void hat_flash_set_spi_hz(uint32_t spi_hz) {
  g_spi_hz = spi_hz;
}

uint32_t hat_flash_get_spi_hz(void) {
  return g_spi_hz;
}

bool hat_flash_release_powerdown(void) {
  if (!g_started) return false;

  flash_begin_transaction();
  g_spi->transfer(CMD_RELEASE_POWERDOWN);
  flash_end_transaction();

  delay(1);
  return true;
}

bool hat_flash_software_reset(void) {
  if (!g_started) return false;

  flash_begin_transaction();
  g_spi->transfer(CMD_ENABLE_RESET);
  flash_end_transaction();

  delayMicroseconds(2);

  flash_begin_transaction();
  g_spi->transfer(CMD_RESET_DEVICE);
  flash_end_transaction();

  delay(1);
  return true;
}

uint8_t hat_flash_read_status1(void) {
  if (!g_started) return 0xFF;

  flash_begin_transaction();
  g_spi->transfer(CMD_READ_STATUS1);
  uint8_t v = g_spi->transfer(0x00);
  flash_end_transaction();
  return v;
}

uint8_t hat_flash_read_status2(void) {
  if (!g_started) return 0xFF;

  flash_begin_transaction();
  g_spi->transfer(CMD_READ_STATUS2);
  uint8_t v = g_spi->transfer(0x00);
  flash_end_transaction();
  return v;
}

uint8_t hat_flash_read_status3(void) {
  if (!g_started) return 0xFF;

  flash_begin_transaction();
  g_spi->transfer(CMD_READ_STATUS3);
  uint8_t v = g_spi->transfer(0x00);
  flash_end_transaction();
  return v;
}

bool hat_flash_write_enable(void) {
  if (!g_started) return false;

  flash_begin_transaction();
  g_spi->transfer(CMD_WRITE_ENABLE);
  flash_end_transaction();

  return (hat_flash_read_status1() & STATUS1_WEL_MASK) != 0;
}

bool hat_flash_write_disable(void) {
  if (!g_started) return false;

  flash_begin_transaction();
  g_spi->transfer(CMD_WRITE_DISABLE);
  flash_end_transaction();

  return (hat_flash_read_status1() & STATUS1_WEL_MASK) == 0;
}

bool hat_flash_wait_while_busy(uint32_t timeout_ms) {
  uint32_t t0 = millis();
  while ((millis() - t0) < timeout_ms) {
    if ((hat_flash_read_status1() & STATUS1_BUSY_MASK) == 0) {
      return true;
    }
    delay(1);
  }
  return false;
}

uint32_t hat_flash_read_jedec_id(void) {
  if (!g_started) return 0;

  flash_begin_transaction();
  g_spi->transfer(CMD_JEDEC_ID);
  uint8_t mfg  = g_spi->transfer(0x00);
  uint8_t type = g_spi->transfer(0x00);
  uint8_t cap  = g_spi->transfer(0x00);
  flash_end_transaction();

  return ((uint32_t)mfg << 16) | ((uint32_t)type << 8) | cap;
}

bool hat_flash_read_unique_id(uint8_t out[8]) {
  if (!g_started || !out) return false;

  flash_begin_transaction();
  g_spi->transfer(CMD_UNIQUE_ID);
  g_spi->transfer(0x00);
  g_spi->transfer(0x00);
  g_spi->transfer(0x00);
  g_spi->transfer(0x00);
  for (int i = 0; i < 8; ++i) {
    out[i] = g_spi->transfer(0x00);
  }
  flash_end_transaction();

  return true;
}

uint32_t hat_flash_size_bytes(void) {
  uint32_t jedec = hat_flash_read_jedec_id();
  return capacity_code_to_bytes((uint8_t)(jedec & 0xFF));
}

bool hat_flash_get_info(hat_flash_info_t* out) {
  if (!out) return false;

  memset(out, 0, sizeof(*out));
  out->jedec_id = hat_flash_read_jedec_id();
  out->manufacturer_id = (uint8_t)((out->jedec_id >> 16) & 0xFF);
  out->memory_type    = (uint8_t)((out->jedec_id >> 8) & 0xFF);
  out->capacity_code  = (uint8_t)(out->jedec_id & 0xFF);
  out->capacity_bytes = capacity_code_to_bytes(out->capacity_code);
  out->unique_id_valid = hat_flash_read_unique_id(out->unique_id);

  return true;
}

bool hat_flash_validate_expected_jedec(uint8_t manufacturer,
                                       uint8_t mem_type,
                                       uint8_t capacity_code) {
  uint32_t jedec = hat_flash_read_jedec_id();
  uint8_t m = (jedec >> 16) & 0xFF;
  uint8_t t = (jedec >> 8) & 0xFF;
  uint8_t c = jedec & 0xFF;
  return (m == manufacturer) && (t == mem_type) && (c == capacity_code);
}

bool hat_flash_is_w25q128jv(void) {
  uint32_t jedec = hat_flash_read_jedec_id();
  uint8_t m = (jedec >> 16) & 0xFF;
  uint8_t t = (jedec >> 8) & 0xFF;
  uint8_t c = jedec & 0xFF;

  if (m != 0xEF) return false;
  if (c != 0x18) return false;

  // Accept the common type bytes we have seen for this family/variant.
  return (t == 0x40) || (t == 0x70);
}

bool hat_flash_read(uint32_t addr, uint8_t* data, size_t len) {
  if (!g_started || !data || len == 0) return false;

  flash_begin_transaction();
  g_spi->transfer(CMD_READ_DATA);
  flash_write_addr24(addr);
  for (size_t i = 0; i < len; ++i) {
    data[i] = g_spi->transfer(0x00);
  }
  flash_end_transaction();

  return true;
}

bool hat_flash_fast_read(uint32_t addr, uint8_t* data, size_t len) {
  if (!g_started || !data || len == 0) return false;

  flash_begin_transaction();
  g_spi->transfer(CMD_FAST_READ);
  flash_write_addr24(addr);
  g_spi->transfer(0x00);  // dummy byte
  for (size_t i = 0; i < len; ++i) {
    data[i] = g_spi->transfer(0x00);
  }
  flash_end_transaction();

  return true;
}

bool hat_flash_read_auto(uint32_t addr, uint8_t* data, size_t len) {
  if (g_spi_hz > FLASH_STANDARD_READ_MAX_HZ) {
    return hat_flash_fast_read(addr, data, len);
  }
  return hat_flash_read(addr, data, len);
}

bool hat_flash_sector_erase_4k(uint32_t addr) {
  if (!g_started) return false;
  if (!hat_flash_write_enable()) return false;

  flash_begin_transaction();
  g_spi->transfer(CMD_SECTOR_ERASE_4K);
  flash_write_addr24(addr & ~(FLASH_SECTOR_SIZE - 1));
  flash_end_transaction();

  return hat_flash_wait_while_busy(3000);
}

bool hat_flash_block_erase_32k(uint32_t addr) {
  if (!g_started) return false;
  if (!hat_flash_write_enable()) return false;

  flash_begin_transaction();
  g_spi->transfer(CMD_BLOCK_ERASE_32K);
  flash_write_addr24(addr & ~((uint32_t)32768 - 1));
  flash_end_transaction();

  return hat_flash_wait_while_busy(8000);
}

bool hat_flash_block_erase_64k(uint32_t addr) {
  if (!g_started) return false;
  if (!hat_flash_write_enable()) return false;

  flash_begin_transaction();
  g_spi->transfer(CMD_BLOCK_ERASE_64K);
  flash_write_addr24(addr & ~((uint32_t)65536 - 1));
  flash_end_transaction();

  return hat_flash_wait_while_busy(12000);
}

bool hat_flash_chip_erase(void) {
  if (!g_started) return false;
  if (!hat_flash_write_enable()) return false;

  flash_begin_transaction();
  g_spi->transfer(CMD_CHIP_ERASE_C7);
  flash_end_transaction();

  return hat_flash_wait_while_busy(120000);
}

bool hat_flash_page_program(uint32_t addr, const uint8_t* data, size_t len) {
  if (!g_started || !data || len == 0 || len > FLASH_PAGE_SIZE) return false;

  size_t page_off = addr % FLASH_PAGE_SIZE;
  if ((page_off + len) > FLASH_PAGE_SIZE) return false;

  if (!hat_flash_write_enable()) return false;

  flash_begin_transaction();
  g_spi->transfer(CMD_PAGE_PROGRAM);
  flash_write_addr24(addr);
  for (size_t i = 0; i < len; ++i) {
    g_spi->transfer(data[i]);
  }
  flash_end_transaction();

  return hat_flash_wait_while_busy(100);
}

bool hat_flash_write(uint32_t addr, const uint8_t* data, size_t len) {
  if (!g_started || !data || len == 0) return false;

  size_t remaining = len;
  size_t index = 0;

  while (remaining > 0) {
    size_t page_off = addr % FLASH_PAGE_SIZE;
    size_t chunk = FLASH_PAGE_SIZE - page_off;
    if (chunk > remaining) chunk = remaining;

    if (!hat_flash_page_program(addr, &data[index], chunk)) {
      return false;
    }

    addr += chunk;
    index += chunk;
    remaining -= chunk;
  }

  return true;
}

bool hat_flash_verify_data(uint32_t addr, const uint8_t* expected, size_t len) {
  if (!expected || len == 0) return false;

  uint8_t buf[256];
  size_t remaining = len;
  size_t offset = 0;

  while (remaining > 0) {
    size_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
    if (!hat_flash_read_auto(addr + offset, buf, chunk)) return false;
    if (memcmp(buf, expected + offset, chunk) != 0) return false;
    remaining -= chunk;
    offset += chunk;
  }

  return true;
}

bool hat_flash_test_pattern(uint32_t addr, size_t len) {
  if (len == 0 || len > FLASH_SECTOR_SIZE) return false;

  uint8_t tx[FLASH_SECTOR_SIZE];
  uint8_t rx[FLASH_SECTOR_SIZE];

  fill_test_pattern(tx, len, addr);
  memset(rx, 0, len);

  if (!hat_flash_sector_erase_4k(addr)) return false;
  if (!hat_flash_write(addr, tx, len)) return false;
  if (!hat_flash_read_auto(addr, rx, len)) return false;

  return memcmp(tx, rx, len) == 0;
}

bool hat_flash_measure_read_speed(uint32_t addr,
                                  size_t len,
                                  size_t chunk_size,
                                  float* kib_per_sec) {
  if (!kib_per_sec || len == 0 || chunk_size == 0) return false;

  uint8_t* buf = (uint8_t*)malloc(chunk_size);
  if (!buf) return false;

  uint32_t t0 = micros();

  size_t remaining = len;
  uint32_t cur_addr = addr;
  while (remaining > 0) {
    size_t chunk = (remaining > chunk_size) ? chunk_size : remaining;
    if (!hat_flash_read_auto(cur_addr, buf, chunk)) {
      free(buf);
      return false;
    }
    cur_addr += chunk;
    remaining -= chunk;
  }

  uint32_t dt = micros() - t0;
  free(buf);

  if (dt == 0) dt = 1;
  *kib_per_sec = ((float)len * 1000000.0f) / ((float)dt * 1024.0f);
  return true;
}

bool hat_flash_measure_write_speed(uint32_t addr,
                                   size_t len,
                                   size_t chunk_size,
                                   float* kib_per_sec) {
  if (!kib_per_sec || len == 0 || chunk_size == 0) return false;
  if ((addr % FLASH_SECTOR_SIZE) != 0) return false;
  if ((len % FLASH_SECTOR_SIZE) != 0) return false;

  uint8_t* tx = (uint8_t*)malloc(chunk_size);
  if (!tx) return false;

  fill_test_pattern(tx, chunk_size, addr);

  uint32_t t0 = micros();

  size_t remaining = len;
  uint32_t cur_addr = addr;

  while (remaining > 0) {
    if (!hat_flash_sector_erase_4k(cur_addr)) {
      free(tx);
      return false;
    }

    size_t sector_remaining = FLASH_SECTOR_SIZE;
    while (sector_remaining > 0) {
      size_t chunk = (sector_remaining > chunk_size) ? chunk_size : sector_remaining;
      fill_test_pattern(tx, chunk, cur_addr + (FLASH_SECTOR_SIZE - sector_remaining));
      if (!hat_flash_write(cur_addr + (FLASH_SECTOR_SIZE - sector_remaining), tx, chunk)) {
        free(tx);
        return false;
      }
      sector_remaining -= chunk;
    }

    cur_addr += FLASH_SECTOR_SIZE;
    remaining -= FLASH_SECTOR_SIZE;
  }

  uint32_t dt = micros() - t0;
  free(tx);

  if (dt == 0) dt = 1;
  *kib_per_sec = ((float)len * 1000000.0f) / ((float)dt * 1024.0f);
  return true;
}

bool hat_flash_speed_sweep(uint32_t test_addr,
                           size_t test_len,
                           const uint32_t* speeds_hz,
                           size_t speed_count,
                           hat_flash_speed_sweep_result_t* out) {
  if (!out || !speeds_hz || speed_count == 0) return false;
  if ((test_addr % FLASH_SECTOR_SIZE) != 0) return false;
  if ((test_len % FLASH_SECTOR_SIZE) != 0) return false;
  if (speed_count > (sizeof(out->points) / sizeof(out->points[0]))) return false;

  memset(out, 0, sizeof(*out));

  uint8_t* tx = (uint8_t*)malloc(test_len);
  uint8_t* rx = (uint8_t*)malloc(test_len);
  if (!tx || !rx) {
    free(tx);
    free(rx);
    return false;
  }

  fill_test_pattern(tx, test_len, test_addr);

  for (size_t i = 0; i < speed_count; ++i) {
    uint32_t hz = speeds_hz[i];
    hat_flash_set_spi_hz(hz);

    hat_flash_release_powerdown();
    hat_flash_software_reset();

    hat_flash_speed_point_t* p = &out->points[out->points_tested];
    memset(p, 0, sizeof(*p));
    p->spi_hz = hz;
    p->used_fast_read = (hz > FLASH_STANDARD_READ_MAX_HZ);

    bool ok = true;

    if (!hat_flash_sector_erase_4k(test_addr)) ok = false;
    if (ok && test_len > FLASH_SECTOR_SIZE) {
      for (uint32_t a = test_addr + FLASH_SECTOR_SIZE; a < (test_addr + test_len); a += FLASH_SECTOR_SIZE) {
        if (!hat_flash_sector_erase_4k(a)) {
          ok = false;
          break;
        }
      }
    }

    if (ok && !hat_flash_write(test_addr, tx, test_len)) ok = false;
    if (ok && !hat_flash_read_auto(test_addr, rx, test_len)) ok = false;
    if (ok && memcmp(tx, rx, test_len) != 0) ok = false;

    p->passed = ok;

    if (ok) {
      hat_flash_measure_read_speed(test_addr, test_len, 1024, &p->read_kib_per_sec);
      hat_flash_measure_write_speed(test_addr, test_len, 256, &p->write_kib_per_sec);
      out->max_pass_hz = hz;
    } else {
      out->first_fail_hz = hz;
      out->found_failure = true;
      out->points_tested++;
      break;
    }

    out->points_tested++;
  }

  free(tx);
  free(rx);
  return true;
}