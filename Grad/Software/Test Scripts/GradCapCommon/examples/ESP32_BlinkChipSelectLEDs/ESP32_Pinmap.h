#pragma once

#include <Arduino.h>

/*
 * ============================================================================
 * File: ESP32_Pinmap.h
 * Project: Grad Cap Display Hardware Definitions
 * Target MCU: ESP32 display nodes
 *
 * Description
 * -----------
 * This header defines the ESP32 GPIO assignments used by the display-node
 * hardware.
 *
 * Purpose
 * -------
 * - Centralize all board-level pin definitions for the ESP32 node boards
 * - Provide a single source of truth for LCD, SPI, RS-485, and other hardware
 *   connections used by the node firmware
 * - Keep GPIO assignments separate from higher-level driver and protocol code
 *
 * Naming convention
 * -----------------
 * - All pin symbols in this file begin with ESP32_ to make it immediately clear
 *   that they are board-local hardware definitions for the ESP32 node hardware.
 * - This file defines constants only. It does not declare functions.
 *
 * Source note
 * -----------
 * These assignments are derived from the earlier HatPins.h file and renamed into
 * the new naming convention.
 * ============================================================================
 */

// -----------------------------------------------------------------------------
// General / Synchronization
// -----------------------------------------------------------------------------

/*
 * ESP32_PIN_FRAME_SYNC_IN
 *
 * External frame-sync input into the ESP32 node.
 */
static constexpr int ESP32_PIN_FRAME_SYNC_IN = 1;

// -----------------------------------------------------------------------------
// LCD RGB Data Bus
// -----------------------------------------------------------------------------

/*
 * Parallel LCD pixel data bus.
 *
 * DB0-DB15 are the primary 16-bit RGB data lines used by the current display
 * implementation. DB16 and DB17 are retained here because they were present in
 * the original board map and may be relevant for future RGB666/RGB888 studies.
 */
static constexpr int ESP32_PIN_LCD_DB0  = 2;
static constexpr int ESP32_PIN_LCD_DB1  = 4;
static constexpr int ESP32_PIN_LCD_DB2  = 5;
static constexpr int ESP32_PIN_LCD_DB3  = 6;
static constexpr int ESP32_PIN_LCD_DB4  = 7;
static constexpr int ESP32_PIN_LCD_DB5  = 13;
static constexpr int ESP32_PIN_LCD_DB6  = 14;
static constexpr int ESP32_PIN_LCD_DB7  = 15;
static constexpr int ESP32_PIN_LCD_DB8  = 16;
static constexpr int ESP32_PIN_LCD_DB9  = 17;
static constexpr int ESP32_PIN_LCD_DB10 = 18;
static constexpr int ESP32_PIN_LCD_DB11 = 19;
static constexpr int ESP32_PIN_LCD_DB12 = 20;
static constexpr int ESP32_PIN_LCD_DB13 = 42;
static constexpr int ESP32_PIN_LCD_DB14 = 47;
static constexpr int ESP32_PIN_LCD_DB15 = 48;
static constexpr int ESP32_PIN_LCD_DB16 = 43;
static constexpr int ESP32_PIN_LCD_DB17 = 44;

// -----------------------------------------------------------------------------
// LCD Timing / Control Pins
// -----------------------------------------------------------------------------

/* LCD pixel clock output. */
static constexpr int ESP32_PIN_LCD_PCLK = 38;

/* LCD data-enable timing signal. */
static constexpr int ESP32_PIN_LCD_DE = 39;

/* LCD vertical sync timing signal. */
static constexpr int ESP32_PIN_LCD_VSYNC = 40;

/* LCD horizontal sync timing signal. */
static constexpr int ESP32_PIN_LCD_HSYNC = 41;

/* LCD reset control pin. */
static constexpr int ESP32_PIN_LCD_RESET = 3;

// -----------------------------------------------------------------------------
// Shared SPI Bus
// -----------------------------------------------------------------------------

/* SPI master-out / slave-in line. */
static constexpr int ESP32_PIN_SPI_MOSI = 8;

/* SPI serial clock. */
static constexpr int ESP32_PIN_SPI_SCK = 9;

/*
 * SPI master-in / slave-out line.
 *
 * This pin is also a strap pin on the ESP32-S3 board design, so it should be
 * handled carefully during early boot.
 */
static constexpr int ESP32_PIN_SPI_MISO = 45;

/*
 * SPI chip-select helper / decoder select input 0.
 *
 * This pin is also a strap pin in the current hardware.
 */
static constexpr int ESP32_PIN_SPI_CS0 = 46;

/* SPI chip-select helper / decoder select input 1. */
static constexpr int ESP32_PIN_SPI_CS1 = 10;

// -----------------------------------------------------------------------------
// Reserved / Special-Use Pins
// -----------------------------------------------------------------------------

/*
 * Reserved for PSRAM in the current ESP32 module/board design.
 * Firmware should not repurpose these signals.
 */
static constexpr int ESP32_PIN_PSRAM_0 = 35;
static constexpr int ESP32_PIN_PSRAM_1 = 36;
static constexpr int ESP32_PIN_PSRAM_2 = 37;

// -----------------------------------------------------------------------------
// RS-485 Bus Interface
// -----------------------------------------------------------------------------

/* UART TX pin feeding the RS-485 transceiver DI input. */
static constexpr int ESP32_PIN_RS485_TX = 11;

/* UART RX pin receiving the RS-485 transceiver RO output. */
static constexpr int ESP32_PIN_RS485_RX = 12;

/*
 * RS-485 driver-enable control.
 *
 * Current hardware convention:
 * - LOW  = receive mode
 * - HIGH = transmit mode
 */
static constexpr int ESP32_PIN_RS485_DE = 21;
