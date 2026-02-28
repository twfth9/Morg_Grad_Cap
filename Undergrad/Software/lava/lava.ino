#include <FastLED.h>

#define NUM_LEDS 84
#define LED_PIN A2

CRGB leds[NUM_LEDS];

CRGBPalette16  lavaPalette = CRGBPalette16(
  CRGB::Black,  CRGB::Black,   CRGB::Black,  CRGB::Black,
  CRGB::Black,  CRGB::Black,   CRGB::Black,  CRGB::Black,
  CRGB::Black,  CRGB::Black,  CRGB::Black,      CRGB::Black,
  CRGB::DarkBlue,    CRGB::LightBlue,   CRGB::LightBlue,      CRGB::White
);

uint16_t brightnessScale = 255;
uint16_t indexScale = 500;

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
  Serial.begin(57600);
}


void loop() {
  
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t brightness = inoise8(i * brightnessScale, millis() / 10);
    uint8_t index = inoise8(i * indexScale, millis() /20);
    leds[i] = ColorFromPalette(lavaPalette, index, brightness);
    //leds[i] = CHSV(0, 255, brightness);
  }

  FastLED.show();
}
