#pragma once
#include <Arduino.h>  // provides pinMode(), digitalWrite(), HIGH/LOW, etc.

/* =======================
   ESP32 Display Board Pins
   ======================= */

#define PIN_FRAME_SYNC_IN   1   /* IO01 */

/* LCD RGB data bus */
#define PIN_LCD_DB0   2
#define PIN_LCD_DB1   4
#define PIN_LCD_DB2   5
#define PIN_LCD_DB3   6
#define PIN_LCD_DB4   7
#define PIN_LCD_DB5   13
#define PIN_LCD_DB6   14
#define PIN_LCD_DB7   15
#define PIN_LCD_DB8   16
#define PIN_LCD_DB9   17
#define PIN_LCD_DB10  18
#define PIN_LCD_DB11  19
#define PIN_LCD_DB12  20
#define PIN_LCD_DB13  42
#define PIN_LCD_DB14  47
#define PIN_LCD_DB15  48
#define PIN_LCD_DB16  43
#define PIN_LCD_DB17  44

/* LCD control/timing */

#define PIN_LCD_PCLK   38
#define PIN_LCD_DE     39
#define PIN_LCD_VSYNC  40
#define PIN_LCD_HSYNC  41
#define PIN_LCD_RESET   3

/* SPI bus */
#define PIN_SPI_MOSI   8
#define PIN_SPI_SCK    9

/* Strapping pins (use after boot) */
#define PIN_SPI_MISO   45  /* IO45 strap */
#define PIN_SPI_CS0    46  /* IO46 strap -> decoder A input */

/* Decoder B input */
#define PIN_SPI_CS1    10  /* IO10 -> decoder B input */

/* Reserved for PSRAM (don’t touch) */
#define PIN_PSRAM_0    35
#define PIN_PSRAM_1    36
#define PIN_PSRAM_2    37

/* RS-485 Bus Pins */
#define PIN_RS485_TX   11  /* UART TX -> THVD1400D DI */
#define PIN_RS485_RX   12  /* THVD1400D RO -> UART RX */
#define PIN_RS485_DE   21  /* THVD1400D DE (HIGH=TX, LOW=RX) */