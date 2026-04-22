/*
 * File: HatSpiMux.cpp
 * Description: Implementation of the SPI target multiplexer control for the ESP32
 * hat hardware. Drives the decoder select pins while respecting strap-pin
 * behavior during boot.
 * Function count: 3 functions
 * Target microcontroller: ESP32
 */

#include "ESP32_SpiMux.h"

namespace {

/*
 * Drive decoder inputs in datasheet terms: A and B.
 *
 * Current board mapping:
 *   A = ESP32_PIN_SPI_CS0  (strap-related pin)
 *   B = ESP32_PIN_SPI_CS1
 *
 * We drive B first and A second so the strap-related pin is changed last.
 */
void setDecoderInputs(uint8_t a_high, uint8_t b_high) {
  digitalWrite(ESP32_PIN_SPI_CS1, b_high ? HIGH : LOW);  // B first
  digitalWrite(ESP32_PIN_SPI_CS0, a_high ? HIGH : LOW);  // A second
}

}  // namespace

void hatSpiMuxBegin() {
  /* Give boot strapping time before actively driving strap-related pins. */
  delay(50);

  pinMode(ESP32_PIN_SPI_CS1, OUTPUT);  // B
  pinMode(ESP32_PIN_SPI_CS0, OUTPUT);  // A

  /* Safe idle state: select the unused/disconnected decoder state. */
  hatSpiMuxDisconnect();
}

void hatSpiMuxSelect(HatSpiTarget target) {
  switch (target) {
    case HatSpiTarget::Flash:
      /* Y0: B=0, A=0 */
      setDecoderInputs(0, 0);
      break;

    case HatSpiTarget::Sd:
      /* Y1: B=1, A=0 */
      setDecoderInputs(0, 1);
      break;

    case HatSpiTarget::Disconnect:
      /* Y2: B=0, A=1 */
      setDecoderInputs(1, 0);
      break;

    case HatSpiTarget::Lcd:
      /* Y3: B=1, A=1 */
      setDecoderInputs(1, 1);
      break;

    default:
      setDecoderInputs(1, 0);
      break;
  }

  /* Very small settle time after changing decoder state. */
  delayMicroseconds(1);
}
