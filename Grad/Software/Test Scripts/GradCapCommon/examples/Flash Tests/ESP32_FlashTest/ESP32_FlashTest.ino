#include <Arduino.h>

#include "Debug485.h"
#include "HatSpiMux.h"
#include "HatFlash.h"

static const uint32_t kSweepHz[] = {
  1000000UL,
  2000000UL,
  4000000UL,
  8000000UL,
  12000000UL,
  16000000UL,
  20000000UL,
  26000000UL,
  32000000UL,
  40000000UL,
  50000000UL,
  60000000UL,
  80000000UL
};

static void print_uid(const uint8_t uid[8]) {
  for (int i = 0; i < 8; ++i) {
    Debug485.printf("%02X", uid[i]);
  }
}

void setup() {
  hat_spi_mux_begin();
  hat_spi_mux_disconnect();

  Debug485.begin(115200);
  delay(200);

  Debug485.printf("\r\n[FLASH] Speed sweep starting...\r\n");

  if (!hat_flash_begin(1000000UL)) {
    Debug485.printf("[FLASH] hat_flash_begin failed\r\n");
    return;
  }

  hat_flash_release_powerdown();
  hat_flash_software_reset();

  hat_flash_info_t info;
  if (hat_flash_get_info(&info)) {
    Debug485.printf("[FLASH] JEDEC: %02X %02X %02X\r\n",
                    info.manufacturer_id,
                    info.memory_type,
                    info.capacity_code);
    Debug485.printf("[FLASH] Capacity: %lu bytes\r\n",
                    (unsigned long)info.capacity_bytes);
    if (info.unique_id_valid) {
      Debug485.printf("[FLASH] UID: ");
      print_uid(info.unique_id);
      Debug485.printf("\r\n");
    }
  }

  Debug485.printf("[FLASH] W25Q128JV family check: %s\r\n",
                  hat_flash_is_w25q128jv() ? "PASS" : "FAIL");

  hat_flash_speed_sweep_result_t result;
  bool ok = hat_flash_speed_sweep(
      0x000000,
      4096,     // one sector
      kSweepHz,
      sizeof(kSweepHz) / sizeof(kSweepHz[0]),
      &result);

  if (!ok) {
    Debug485.printf("[FLASH] Speed sweep call failed\r\n");
    return;
  }

  for (size_t i = 0; i < result.points_tested; ++i) {
    const hat_flash_speed_point_t* p = &result.points[i];
    Debug485.printf("[FLASH] %8lu Hz  %s  read=%0.1f KiB/s  write=%0.1f KiB/s  mode=%s\r\n",
                    (unsigned long)p->spi_hz,
                    p->passed ? "PASS" : "FAIL",
                    p->read_kib_per_sec,
                    p->write_kib_per_sec,
                    p->used_fast_read ? "FAST(0Bh)" : "READ(03h)");
  }

  Debug485.printf("[FLASH] Max passing SPI clock: %lu Hz\r\n",
                  (unsigned long)result.max_pass_hz);

  if (result.found_failure) {
    Debug485.printf("[FLASH] First failing SPI clock: %lu Hz\r\n",
                    (unsigned long)result.first_fail_hz);
  } else {
    Debug485.printf("[FLASH] No failures encountered in sweep.\r\n");
  }

  Debug485.printf("[FLASH] Speed sweep complete.\r\n");
}

void loop() {
  delay(1000);
}