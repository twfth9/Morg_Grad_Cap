/*
 * File: HatSpiMux.c
 * Description: Implementation of the SPI target multiplexer control for the hat hardware. Drives the decoder select pins while respecting ESP32 strap-pin behavior.
 * Function count: 3 functions
 * Target microcontroller: ESP32
 */

#include "HatSpiMux.h"

/* Set decoder inputs in datasheet terms: A (CS0) and B (CS1) */
/* Drive the decoder A/B select inputs to choose one SPI target state. */
static void set_ab(uint8_t A_high, uint8_t B_high)
{
  /* Drive B first (non-strap), then A (strap) */
  digitalWrite(PIN_SPI_CS1, B_high ? HIGH : LOW); /* B */
  digitalWrite(PIN_SPI_CS0, A_high ? HIGH : LOW); /* A */
}

/* Initialize the SPI mux select pins after boot and enter the disconnected idle state. */
void hat_spi_mux_begin(void)
{
  /* Give boot strapping time before driving strap pins */
  delay(50);

  pinMode(PIN_SPI_CS1, OUTPUT); /* B */
  pinMode(PIN_SPI_CS0, OUTPUT); /* A (strap) */

  /* Safe idle: select Y2 (unused/disconnected) => B=0, A=1 */
  hat_spi_mux_disconnect();
}

/* Select which peripheral is connected to the shared SPI bus. */
void hat_spi_mux_select(spi_target_t target)
{
  switch (target) {
    case SPI_TARGET_FLASH:
      /* Y0: B=0, A=0 */
      set_ab(0, 0);
      break;

    case SPI_TARGET_SD:
      /* Y1: B=1, A=0 */
      set_ab(0, 1);
      break;

    case SPI_TARGET_DISCONNECT:
      /* Y2: B=0, A=1 (unused output) */
      set_ab(1, 0);
      break;

    case SPI_TARGET_LCD:
      /* Y3: B=1, A=1 */
      set_ab(1, 1);
      break;

    default:
      set_ab(1, 0);
      break;
  }

  /* Tiny settle time */
  delayMicroseconds(1);
}
