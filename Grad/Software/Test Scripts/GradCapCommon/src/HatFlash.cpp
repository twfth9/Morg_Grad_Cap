#include "HatFlash.h"

#include "HatPins.h"
#include "HatSpiMux.h"

static SPIClass* g_spi = &SPI;
static bool g_started = false;
static uint32_t g_spi_hz = 8000000;  // conservative default for bring-up

// Winbond W25Q command set
#define CMD_WRITE_ENABLE       0x06
#define CMD_WRITE_DISABLE      0x04
#define CMD_READ_STATUS1       0x05
#define CMD_READ_STATUS2       0x35
#define CMD_READ_STATUS3       0x15
#define CMD_READ_DATA          0x03
#define CMD_FAST_READ          0x0B
#define CMD_PAGE_PROGRAM       0x02
#define CMD_SECTOR_ERASE_4K    0x20
#define CMD_BLOCK_ERASE_32K    0x52
#define CMD_BLOCK_ERASE_64K    0xD8
#define CMD_CHIP_ERASE_60      0x60
#define CMD_CHIP_ERASE_C7      0xC7
#define CMD_JEDEC_ID           0x9F
#define CMD_UNIQUE_ID          0x4B

#define STATUS1_BUSY_MASK      0x01
#define STATUS1_WEL_MASK       0x02

#define FLASH_PAGE_SIZE        256UL
#define FLASH_SECTOR_SIZE      4096UL

static inline void flash_select(void) {
  hat_spi_mux_select_flash();
}

static inline void flash_deselect(void) {
  hat_spi_mux_disconnect();
}

static inline SPISettings flash_spi_settings(void) {
  return SPISettings(g_spi_hz, MSBFIRST, SPI_MODE0);
}

static inline void flash_begin_transaction(void) {
  flash_select();
  g_spi->beginTransaction(flash_spi_settings());
}

static inline void flash_end_transaction(void) {
  g_spi->endTransaction();
  flash_deselect();
}

static void flash_write_addr24(uint32_t addr) {
  g_spi->transfer((addr >> 16) & 0xFF);
  g_spi->transfer((addr >> 8) & 0xFF);
  g_spi->transfer(addr & 0xFF);
}

static uint32_t capacity_code_to_bytes(uint8_t code) {
  // Winbond capacity byte is typically log2(bytes)
  // e.g. 0x18 => 2^24 bytes => 16 MiB
  if (code < 0x10 || code > 0x30) {
    return 0;
  }
  return (1UL << code);
}

bool hat_flash_begin(uint32_t spi_hz) {
  g_spi_hz = spi_hz;
  g_spi->begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);
  flash_deselect();
  g_started = true;
  delay(1);
  return true;
}

void hat_flash_end(void) {
  if (!g_started) return;
  g_spi->end();
  g_started = false;
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

  uint8_t sr1 = hat_flash_read_status1();
  return (sr1 & STATUS1_WEL_MASK) != 0;
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
  if (!g_started || out == nullptr) return false;

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
  uint8_t cap = jedec & 0xFF;
  return capacity_code_to_bytes(cap);
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

bool hat_flash_read(uint32_t addr, uint8_t* data, size_t len) {
  if (!g_started || data == nullptr || len == 0) return false;

  flash_begin_transaction();
  g_spi->transfer(CMD_READ_DATA);
  flash_write_addr24(addr);
  for (size_t i = 0; i < len; ++i) {
    data[i] = g_spi->transfer(0x00);
  }
  flash_end_transaction();

  return true;
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

  // full chip erase can take a while
  return hat_flash_wait_while_busy(120000);
}

bool hat_flash_page_program(uint32_t addr, const uint8_t* data, size_t len) {
  if (!g_started || data == nullptr || len == 0 || len > FLASH_PAGE_SIZE) {
    return false;
  }

  size_t offset_in_page = addr % FLASH_PAGE_SIZE;
  if ((offset_in_page + len) > FLASH_PAGE_SIZE) {
    return false; // do not allow cross-page writes here
  }

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
  if (!g_started || data == nullptr || len == 0) return false;

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

bool hat_flash_test_pattern(uint32_t addr, size_t len) {
  if (len == 0 || len > FLASH_SECTOR_SIZE) return false;

  uint8_t tx[FLASH_SECTOR_SIZE];
  uint8_t rx[FLASH_SECTOR_SIZE];

  for (size_t i = 0; i < len; ++i) {
    tx[i] = (uint8_t)((i * 37u + 13u) & 0xFF);
    rx[i] = 0;
  }

  if (!hat_flash_sector_erase_4k(addr)) return false;
  if (!hat_flash_write(addr, tx, len)) return false;
  if (!hat_flash_read(addr, rx, len)) return false;

  return (memcmp(tx, rx, len) == 0);
}

bool hat_flash_measure_read_speed(uint32_t addr,
                                  size_t len,
                                  size_t chunk_size,
                                  float* kb_per_sec) {
  if (!g_started || chunk_size == 0 || kb_per_sec == nullptr) return false;

  uint8_t* buf = (uint8_t*)malloc(chunk_size);
  if (!buf) return false;

  uint32_t start_ms = millis();
  size_t done = 0;

  while (done < len) {
    size_t chunk = chunk_size;
    if (chunk > (len - done)) chunk = len - done;
    if (!hat_flash_read(addr + done, buf, chunk)) {
      free(buf);
      return false;
    }
    done += chunk;
  }

  uint32_t elapsed = millis() - start_ms;
  free(buf);

  if (elapsed == 0) elapsed = 1;
  *kb_per_sec = ((float)len / 1024.0f) / ((float)elapsed / 1000.0f);
  return true;
}

bool hat_flash_measure_write_speed(uint32_t addr,
                                   size_t len,
                                   size_t chunk_size,
                                   float* kb_per_sec) {
  if (!g_started || chunk_size == 0 || kb_per_sec == nullptr) return false;

  uint8_t* buf = (uint8_t*)malloc(chunk_size);
  if (!buf) return false;

  for (size_t i = 0; i < chunk_size; ++i) {
    buf[i] = (uint8_t)(i & 0xFF);
  }

  // erase enough 4k sectors for the target region
  uint32_t start_addr = addr & ~(FLASH_SECTOR_SIZE - 1);
  uint32_t end_addr   = (addr + len + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);

  for (uint32_t a = start_addr; a < end_addr; a += FLASH_SECTOR_SIZE) {
    if (!hat_flash_sector_erase_4k(a)) {
      free(buf);
      return false;
    }
  }

  uint32_t start_ms = millis();
  size_t done = 0;

  while (done < len) {
    size_t chunk = chunk_size;
    if (chunk > (len - done)) chunk = len - done;
    if (!hat_flash_write(addr + done, buf, chunk)) {
      free(buf);
      return false;
    }
    done += chunk;
  }

  uint32_t elapsed = millis() - start_ms;
  free(buf);

  if (elapsed == 0) elapsed = 1;
  *kb_per_sec = ((float)len / 1024.0f) / ((float)elapsed / 1000.0f);
  return true;
}