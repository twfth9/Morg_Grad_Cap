/*
 * File: HatSpiMux.h
 * Description: C-style interface for the SPI target multiplexer that selects the flash, SD card, LCD sideband, or a disconnected idle state on the shared SPI bus.
 * Function count: 2 public API functions, 4 inline convenience helpers, and 1 enum
 * Target microcontroller: ESP32
 */

#pragma once
#include <Arduino.h>
#include "HatPins.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
  SN74LVC1G139 mapping (per your datasheet wording):

    Inputs:
      A = ESP_CS0 = PIN_SPI_CS0  (strap pin IO46)
      B = ESP_CS1 = PIN_SPI_CS1

    Outputs:
      Y0 -> ESP_CS_FLASH (active-low)
      Y1 -> ESP_uSD_BUS_CONNECT (active-low)
      Y2 -> ESP_CS_uSD (active-low) [HARDWARE MOD: disconnected]
      Y3 -> ESP_CS_LCD  (active-low)

  HARDWARE MOD:
    - Y2 is physically disconnected
    - Y1 is tied to BOTH uSD_BUS_CONNECT and uSD_CS
    - Therefore: selecting Y1 asserts BOTH connect+CS low.

  We use the now-unused Y2 state (B=1, A=0) as "DISCONNECT".
  Decoder selection truth table (active-low one-hot):
    B A:
    0 0 -> Y0 (FLASH)
    0 1 -> Y1 (SD connect + SD CS)
    1 0 -> Y2 (DISCONNECT / unused)
    1 1 -> Y3 (LCD)
*/

typedef enum {
  SPI_TARGET_FLASH = 0,
  SPI_TARGET_SD    = 1,
  SPI_TARGET_DISCONNECT = 2,
  SPI_TARGET_LCD   = 3
} spi_target_t;

/* Call once in setup(), after boot */
/* Initialize the SPI target multiplexer after boot and drive it to a safe disconnected idle state.
   Input: None.
   Output: None. */
void hat_spi_mux_begin(void);

/* Select one of the four states above */
/* Select which SPI target is connected to the shared bus.
   Input: Desired SPI target enum value.
   Output: None. */
void hat_spi_mux_select(spi_target_t target);

/* Convenience helpers */
/* Convenience helper that connects the external flash chip to the shared SPI bus.
   Input: None.
   Output: None. */
static inline void hat_spi_mux_select_flash(void) { hat_spi_mux_select(SPI_TARGET_FLASH); }
/* Convenience helper that connects the microSD path to the shared SPI bus.
   Input: None.
   Output: None. */
static inline void hat_spi_mux_select_sd(void)    { hat_spi_mux_select(SPI_TARGET_SD); }
/* Convenience helper that disconnects all active SPI peripherals from the shared bus.
   Input: None.
   Output: None. */
static inline void hat_spi_mux_disconnect(void)   { hat_spi_mux_select(SPI_TARGET_DISCONNECT); }
/* Convenience helper that connects the LCD serial sideband interface to the shared SPI bus.
   Input: None.
   Output: None. */
static inline void hat_spi_mux_select_lcd(void)   { hat_spi_mux_select(SPI_TARGET_LCD); }

#ifdef __cplusplus
} // extern "C"
#endif
