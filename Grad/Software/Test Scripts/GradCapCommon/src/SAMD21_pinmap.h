/*
 * File: SAMD21_pinmap.h
 * Description: Pin map for the SAMD21 companion microcontroller board. Defines SPI, RS-485, button, frame-sync, microSD control, and NeoPixel connections.
 * Function count: 0 functions; pin definitions only
 * Target microcontroller: SAMD21
 */

#pragma once

/* Pin map only: this file contains SAMD21 board-level GPIO assignments and does not declare callable functions. */

#ifdef __cplusplus
extern "C" {
#endif

#define SAMD_MOSI             (A10)  /* PA10 */
#define SAMD_MISO             (A9)   /* PA09 */
#define SAMD_SCK              (A8)   /* PA11 */

#define SAMD_RS485_RX_PIN     (A7)   /* PA07 */
#define SAMD_RS485_TX_PIN     (A6)   /* PA06 */

#define BACKLIGHT_PWM_OUT     (5)    /* PA17 */
#define SAMD_RS485_DE_PIN     (4)    /* PA16  (QT Py labeled SDA) */

#define BUTTON_SENSE          (A3)   /* PA05 */
#define FRAME_SYNC_OUT        (A2)   /* PA04 */
#define SAMD_CS_uSD           (A1)   /* PA03 */
#define SAMD_uSD_BUS_CONNECT  (A0)   /* PA02 */

#define NEOPIXEL_DATA         (11)   /* PA18 */
#define NEOPIXEL_POWER        (12)   /* PA15 */

#ifdef __cplusplus
}
#endif
