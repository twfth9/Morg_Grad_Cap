/*
 * File: HatSpiMux.h
 * Description: C++ interface for the SPI target multiplexer that selects the
 * flash, SD card, LCD sideband, or a disconnected idle state on the shared SPI
 * bus.
 * Function count: 2 public API functions, 4 inline convenience helpers, and
 * 1 enum
 * Target microcontroller: ESP32
 */

#pragma once

#include <Arduino.h>
#include "ESP32_Pinmap.h"

/*
  Decoder mapping carried over from the earlier hardware notes:

    Inputs:
      A = ESP32_PIN_SPI_CS0   (strap pin)
      B = ESP32_PIN_SPI_CS1

    Outputs:
      Y0 -> FLASH select
      Y1 -> microSD connect/select path
      Y2 -> DISCONNECT / unused hardware state
      Y3 -> LCD sideband SPI select

  Selection truth table (active-low one-hot style decoder state):

    B A
    0 0 -> Y0 (FLASH)
    0 1 -> Y1 (SD)
    1 0 -> Y2 (DISCONNECT / unused)
    1 1 -> Y3 (LCD)

  Note:
  - The exact external hardware behavior should still match the board wiring and
    any hardware rework already performed.
  - This library only drives the decoder select pins into the intended states.
*/

enum class HatSpiTarget : uint8_t {
  Flash = 0,
  Sd = 1,
  Disconnect = 2,
  Lcd = 3
};

/*
 * Initialize the SPI target multiplexer after boot and drive it to a safe
 * disconnected idle state.
 */
void hatSpiMuxBegin();

/*
 * Select which SPI target is connected to the shared bus.
 */
void hatSpiMuxSelect(HatSpiTarget target);

/* Convenience helpers */
static inline void hatSpiMuxSelectFlash()      { hatSpiMuxSelect(HatSpiTarget::Flash); }
static inline void hatSpiMuxSelectSd()         { hatSpiMuxSelect(HatSpiTarget::Sd); }
static inline void hatSpiMuxDisconnect()       { hatSpiMuxSelect(HatSpiTarget::Disconnect); }
static inline void hatSpiMuxSelectLcd()        { hatSpiMuxSelect(HatSpiTarget::Lcd); }
