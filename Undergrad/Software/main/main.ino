#include <FastLED.h>
#include <OneButton.h>

#define NUM_LEDS  84
#define LED_PIN   A2
#define BTN_PIN   A3       


CRGB leds[NUM_LEDS];
uint8_t patternCounter = 0;

unsigned long startMillis;
unsigned long currentMillis;

OneButton btn = OneButton(BTN_PIN, true, true);

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(100);
  //Serial.begin(57600);

  for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Black;
    }
  FastLED.show();
  btn.attachClick(nextPattern);
  startMillis = millis();
}

void loop() {
  
  switch (patternCounter) {
   case 0:
     FastLED.setBrightness(140);
     starryNight();
     break;
   case 1:
     FastLED.setBrightness(255);
     partyMode2();
     break;
   case 2:
     FastLED.setBrightness(255);
     callsign();
     break;
   case 3:
     FastLED.setBrightness(255);
     partyMode1();
     break;
    }
  
  FastLED.show();
  btn.tick();
}

void nextPattern() {
  patternCounter = (patternCounter + 1) % 4; // MAKE SURE TO SET THIS TO THE NUMBER OF CASES
  // Clear screen, set all pixels to black
  for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::Black;
      }
}

uint16_t partyBrightnessScale = 50000;
uint16_t partyIndexScale = 50000;

CRGBPalette32 partyPalette = CRGBPalette32(
  0xff0000,
  0x0f0000,
  0x00ff00,
  0x000f00,
  0x000ff0,
  0x0000f0,
  0x00000f,
  0xffffff,
  0xff0000,
  0x0f0000,
  0x00ff00,
  0x000f00,
  0x000ff0,
  0x0000f0,
  0x00000f,
  0x000000
);

void partyMode1() {
  for (int i = 0; i < NUM_LEDS; i++) {
    uint16_t brightness = inoise8(i * partyBrightnessScale, millis());
    uint16_t index = inoise8(i * partyIndexScale, millis()/5);
    leds[i] = ColorFromPalette(partyPalette, index, brightness);
  }
}

void partyMode2() {
  uint16_t beatA = beatsin16(15, 0, 255);
  uint16_t beatB = beatsin16(200, 0, 255);
  fill_rainbow(leds, NUM_LEDS, (beatA+beatB)/2, 25);
}
//-------------------------------------------
//-------StarryNight Pattern------------------
//-------------------------------------------

CRGBPalette256 nightPalette = CRGBPalette256(
    0xffffff,
    0xadd8e6,
    0x0c2027,
    0x00004e, 
    0x00001a,
    0x000000,
    0x000000,
    0x000000,
    0x000000,
    0x000000,
    0x000000,
    0x00001a, 
    0x13004d,
    0x0c0c27,
    0xadade6,
    0xffff99
);

uint16_t nightBrightnessScale = 50000;
uint16_t nightIndexScale = 50000;

void starryNight() {
  for (int i = 0; i < NUM_LEDS; i++) {
    uint16_t brightness = inoise8(i * nightBrightnessScale, millis() / 2);
    uint16_t index = inoise8(i * nightIndexScale, millis() / 35);
    leds[i] = ColorFromPalette(nightPalette, index, brightness);
  }
}

//-------------------------------------------
//---------Callsign Pattern------------------
//-------------------------------------------

const uint32_t letter_K[84] = {
//         D1        D2        D3        D4        D5        D6
      0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000,
//   D7        D8        D9        D10       D11       D12       D13
  0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//       D14       D15       D16       D17       D18       D19
      0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000,
//   D20       D21       D22       D23       D24       D25       D26
  0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000,
//       D27       D28       D29       D30       D31       D32
      0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000,
//   D33       D34       D35       D36       D37       D38       D39
  0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000,
//       D40       D41       D42       D43       D44       D45
      0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000,
//   D46       D47       D48       D49       D50       D51       D52
  0xFF0000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000,
//       D53       D54       D55       D56       D57       D58
      0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000,
//   D59       D60       D61       D62       D63       D64       D65
  0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000,
//       D66       D67       D68       D69       D70       D71
      0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000,
//   D72       D73       D74       D75       D76       D77       D78
  0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000,
//       D79       D80       D81       D82       D83       D84
      0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000
};

const uint32_t letter_I[84] = {
//         D1        D2        D3        D4        D5        D6
      0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000,
//   D7        D8        D9        D10       D11       D12       D13
  0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000,
//       D14       D15       D16       D17       D18       D19
      0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000,
//   D20       D21       D22       D23       D24       D25       D26
  0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0xFF0000,
//       D27       D28       D29       D30       D31       D32
      0xFF0000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//   D33       D34       D35       D36       D37       D38       D39
  0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000,
//       D40       D41       D42       D43       D44       D45
      0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000,
//   D46       D47       D48       D49       D50       D51       D52
  0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//       D53       D54       D55       D56       D57       D58
      0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0xFF0000,
//   D59       D60       D61       D62       D63       D64       D65
  0xFF0000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000,
//       D66       D67       D68       D69       D70       D71
      0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000,
//   D72       D73       D74       D75       D76       D77       D78
  0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000,
//       D79       D80       D81       D82       D83       D84
      0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000
};

const uint32_t letter_5[84] = {
//         D1        D2        D3        D4        D5        D6
      0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000,
//   D7        D8        D9        D10       D11       D12       D13
  0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000,
//       D14       D15       D16       D17       D18       D19
      0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
//   D20       D21       D22       D23       D24       D25       D26
  0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000,
//       D27       D28       D29       D30       D31       D32
      0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
//   D33       D34       D35       D36       D37       D38       D39
  0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0xFF0000,
//       D40       D41       D42       D43       D44       D45
      0xFF0000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000,
//   D46       D47       D48       D49       D50       D51       D52
  0x000000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0x000000,
//       D53       D54       D55       D56       D57       D58
      0x000000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000,
//   D59       D60       D61       D62       D63       D64       D65
  0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
//       D66       D67       D68       D69       D70       D71
      0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000,
//   D72       D73       D74       D75       D76       D77       D78
  0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000,
//       D79       D80       D81       D82       D83       D84
      0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000
};

const uint32_t letter_S[84] = {
//         D1        D2        D3        D4        D5        D6
      0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//   D7        D8        D9        D10       D11       D12       D13
  0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000,
//       D14       D15       D16       D17       D18       D19
      0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000,
//   D20       D21       D22       D23       D24       D25       D26
  0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000,
//       D27       D28       D29       D30       D31       D32
      0xFF0000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000,
//   D33       D34       D35       D36       D37       D38       D39
  0x000000, 0xFF0000, 0xFF0000, 0xFF0000, 0x000000, 0xFF0000, 0xFF0000,
//       D40       D41       D42       D43       D44       D45
      0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000,
//   D46       D47       D48       D49       D50       D51       D52
  0xFF0000, 0xFF0000, 0x000000, 0xFF0000, 0xFF0000, 0xFF0000, 0x000000,
//       D53       D54       D55       D56       D57       D58
      0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000,
//   D59       D60       D61       D62       D63       D64       D65
  0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
//       D66       D67       D68       D69       D70       D71
      0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000,
//   D72       D73       D74       D75       D76       D77       D78
  0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000,
//       D79       D80       D81       D82       D83       D84
      0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000
};

const uint32_t letter_X[84] = {
//         D1        D2        D3        D4        D5        D6
      0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//   D7        D8        D9        D10       D11       D12       D13
  0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//       D14       D15       D16       D17       D18       D19
      0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//   D20       D21       D22       D23       D24       D25       D26
  0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//       D27       D28       D29       D30       D31       D32
      0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//   D33       D34       D35       D36       D37       D38       D39
  0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000,
//       D40       D41       D42       D43       D44       D45
      0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000,
//   D46       D47       D48       D49       D50       D51       D52
  0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//       D53       D54       D55       D56       D57       D58
      0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000,
//   D59       D60       D61       D62       D63       D64       D65
  0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//       D66       D67       D68       D69       D70       D71
      0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000,
//   D72       D73       D74       D75       D76       D77       D78
  0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//       D79       D80       D81       D82       D83       D84
      0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000
};

const uint32_t letter_Y[84] = {
//         D1        D2        D3        D4        D5        D6
      0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
//   D7        D8        D9        D10       D11       D12       D13
  0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000,
//       D14       D15       D16       D17       D18       D19
      0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000,
//   D20       D21       D22       D23       D24       D25       D26
  0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000,
//       D27       D28       D29       D30       D31       D32
      0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//   D33       D34       D35       D36       D37       D38       D39
  0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0x000000, 0x000000,
//       D40       D41       D42       D43       D44       D45
      0x000000, 0x000000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000,
//   D46       D47       D48       D49       D50       D51       D52
  0x0000, 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//       D53       D54       D55       D56       D57       D58
      0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//   D59       D60       D61       D62       D63       D64       D65
  0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//       D66       D67       D68       D69       D70       D71
      0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//   D72       D73       D74       D75       D76       D77       D78
  0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000,
//       D79       D80       D81       D82       D83       D84
      0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000
};

#define DIT 1
#define DAH 9
#define LETTER_SPACE 7
#define WORD_SPACE 17
#define PHRASE_SPACE 25

uint8_t callsign_position = 0;
uint8_t unit_speed = 25;

uint8_t ki5sxy[44] = {
  DAH, LETTER_SPACE,
  DIT, LETTER_SPACE,
  DAH, WORD_SPACE,

  DIT, LETTER_SPACE,
  DIT, WORD_SPACE,

  DIT, LETTER_SPACE,
  DIT, LETTER_SPACE,
  DIT, LETTER_SPACE,
  DIT, LETTER_SPACE,
  DIT, WORD_SPACE,

  DIT, LETTER_SPACE,
  DIT, LETTER_SPACE,
  DIT, WORD_SPACE,
  
  DAH, LETTER_SPACE,
  DIT, LETTER_SPACE,
  DIT, LETTER_SPACE,
  DAH, WORD_SPACE,

  DAH, LETTER_SPACE,
  DIT, LETTER_SPACE,
  DAH, LETTER_SPACE,
  DAH, WORD_SPACE,
  
  PHRASE_SPACE, WORD_SPACE
};

void callsign() {
  currentMillis = millis();

  if(currentMillis - startMillis >= (unit_speed * ki5sxy[callsign_position]))
  {
  //else if(callsign_position % 2) { // Red for letters
    switch (callsign_position) {
      case 1:
      case 3:
      case 43:
        for (int i = 0; i < NUM_LEDS; i++) {
              leds[i] = letter_K[i];
        }
        break;

      case 5:
      case 7:
        for (int i = 0; i < NUM_LEDS; i++) {
              leds[i] = letter_I[i];
        }
        break;

      case 9:
      case 11:
      case 13:
      case 15:
      case 17:
        for (int i = 0; i < NUM_LEDS; i++) {
              leds[i] = letter_5[i];
        }
        break;

      case 19:
      case 21:
      case 23:
        for (int i = 0; i < NUM_LEDS; i++) {
              leds[i] = letter_S[i];
        }
        break;

      case 25:
      case 27:
      case 29:
      case 31:
        for (int i = 0; i < NUM_LEDS; i++) {
              leds[i] = letter_X[i];
        }
        break;

      case 33:
      case 35:
      case 37:
      case 39:
        for (int i = 0; i < NUM_LEDS; i++) {
              leds[i] = letter_Y[i];
        }
        break;

      case 42:
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::Black;
        }
        break; 

      default:
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::Black;
        }
        break;
    }
    callsign_position = (callsign_position + 1) % 44;
    startMillis = currentMillis;
  }
  delay(100); // I have no idea why this fixes blinking, something weird with millis() that I don't understand
}



//uint8_t sos[20] = {
//  DIT, LETTER_SPACE,
//  DIT, LETTER_SPACE,
//  DIT, WORD_SPACE,
//
//  DAH, LETTER_SPACE,
//  DAH, LETTER_SPACE,
//  DAH, WORD_SPACE,
//
//  DIT, LETTER_SPACE,
//  DIT, LETTER_SPACE,
//  DIT, WORD_SPACE,
//
//  PHRASE_SPACE, WORD_SPACE
//};

//void callsign() {
//  currentMillis = millis();
//
//  if(currentMillis - startMillis >= (unit_speed * sos[callsign_position]))
//  {
//  //else if(callsign_position % 2) { // Red for letters
//    switch (callsign_position) {
//      case 1:
//      case 3:
//      case 5:
//      case 7:
//      case 9:
//      case 11:
//      case 13:
//      case 15:
//      case 19:
//        for (int i = 0; i < NUM_LEDS; i++) {
//              leds[i] = CRGB::Red;
//        }
//        break;
//      
//      case 18:
//        for (int i = 0; i < NUM_LEDS; i++) {
//              leds[i] = CRGB::Blue;
//        }
//        break;
//
//      default:
//        for (int i = 0; i < NUM_LEDS; i++) {
//          leds[i] = CRGB::Purple;
//        }
//        break;
//    }
//    callsign_position = (callsign_position + 1) % 20;
//    startMillis = currentMillis;
//  }
//  delay(100); // I have no idea why this fixes blinking, something weird with millis() that I don't understand
//}

//uint8_t pixel_index = 0;
//uint8_t color_index = 0;
//CRGBPalette16  pixelChaseColors = CRGBPalette16(
//  CRGB::White,    CRGB::Black,  CRGB::Green,  CRGB::Black,
//  CRGB::Purple,   CRGB::Black,  CRGB::Blue,   CRGB::Black,
//  CRGB::Red,      CRGB::Black,  CRGB::Yellow, CRGB::Black,
//  CRGB::DarkBlue, CRGB::Black,  CRGB::Orange, CRGB::Black
//);
//
//void colorWipe() {
//  currentMillis = millis();
//  Serial.print("CurrentMillis: ");
//  Serial.println(currentMillis);
//  Serial.print("startMillis: ");
//  Serial.println(startMillis);
//  Serial.print("Pixel Index: ");
//  Serial.println(pixel_index);
//  Serial.print("Color Index: ");
//  Serial.println(color_index);
//  if(currentMillis - startMillis >= 1)
//  {
//    leds[pixel_index] = pixelChaseColors[color_index];
//    
//    if(pixel_index == (NUM_LEDS - 1))
//    {
//      color_index = (color_index + 1) % 16;
//    }
//    pixel_index = (pixel_index + 1) % NUM_LEDS;
//    
//    startMillis = currentMillis;
//  }
//}
//
//bool direction = 1;
//
//void pixelChase() {
//  currentMillis = millis();
//  Serial.print("CurrentMillis: ");
//  Serial.println(currentMillis);
//  Serial.print("startMillis: ");
//  Serial.println(startMillis);
//  Serial.print("Pixel Index: ");
//  Serial.println(pixel_index);
//  Serial.print("Color Index: ");
//  Serial.println(color_index);
//  if(currentMillis - startMillis >= 100)
//  {
//    //for (int i = 0; i < NUM_LEDS; i++) {
//    //  leds[i] = CRGB::Black;
//    //}
//    //
//    //  leds[(pixel_index) % NUM_LEDS] = CRGB::White;
//    //  leds[(pixel_index + 1) % NUM_LEDS] = CRGB::White;
//    //  leds[(pixel_index - 1) % NUM_LEDS] = CRGB::White;
//    //
//    //  if(pixel_index == (NUM_LEDS - 1))
//    //  {
//    //    color_index = (color_index + 1) % 16;
//    //    direction = !direction;
//    //  }
//    //  pixel_index = (pixel_index + 1) % NUM_LEDS;
//    //  
//    //  startMillis = currentMillis;
//
//    for (int i = 0; i < NUM_LEDS; i++) {
//      leds[i] = CRGB::Black;
//    }
//
//      leds[(pixel_index) % NUM_LEDS] = CRGB::White;
//      leds[(pixel_index + 1) % NUM_LEDS] = CRGB::White;
//      leds[(pixel_index - 1) % NUM_LEDS] = CRGB::White;
//
//      if(pixel_index == (NUM_LEDS - 1))
//      {
//        color_index = (color_index + 1) % 16;
//        direction = !direction;
//      }
//      pixel_index = (pixel_index + 1) % NUM_LEDS;
//
//      startMillis = currentMillis;
//
//  }
//}
