#include <Arduino.h>
#include "HatPins.h"
#include "HatSpiMux.h"
#include "LCD.h"

static void print_banner(void)
{
  Serial.println();
  Serial.println("========================================");
  Serial.println(" ST7701S LCD Sideband SPI Test Starting ");
  Serial.println("========================================");
  Serial.println();
}

static void test_display_on_off(void)
{
  Serial.println("[TEST] Display OFF");
  ST7701S_DisplayOff();
  delay(1500);

  Serial.println("[TEST] Display ON");
  ST7701S_DisplayOn();
  delay(1500);
}

static void test_inversion(void)
{
  Serial.println("[TEST] Inversion OFF");
  ST7701S_InversionOff();
  delay(1500);

  Serial.println("[TEST] Inversion ON");
  ST7701S_InversionOn();
  delay(1500);
}

static void test_rotation(void)
{
  Serial.println("[TEST] Rotation 180");
  ST7701S_SetRotation180();
  delay(1500);

  Serial.println("[TEST] Rotation 0");
  ST7701S_SetRotation0();
  delay(1500);
}

static void blink_LCD_CS(void)
{
    
    Serial.println("[TEST] Blinking LCD Chip Select");

    lcd_select();
    Serial.println("[TEST] CS ENABLED");
    delay(500);
    lcd_deselect();
    Serial.println("[TEST] CS DISABLED");
    delay(500);

    lcd_select();
    Serial.println("[TEST] CS ENABLED");
    delay(500);
    lcd_deselect();
    Serial.println("[TEST] CS DISABLED");
    delay(500);
}

static void test_color_mode(void)
{
  Serial.println("[TEST] Switch to RGB666");
  ST7701S_SetColorMode(ST7701_COLOR_MODE_RGB666);
  delay(1500);

  Serial.println("[TEST] Switch back to RGB565");
  ST7701S_SetColorMode(ST7701_COLOR_MODE_RGB565);
  delay(1500);
}

static void test_sleep_mode(void)
{
  Serial.println("[TEST] Sleep IN");
  ST7701S_SleepIn();
  delay(1500);

  Serial.println("[TEST] Sleep OUT");
  ST7701S_SleepOut();
  delay(1500);

  Serial.println("[TEST] Display ON after Sleep OUT");
  ST7701S_DisplayOn();
  delay(1500);
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  print_banner();

  Serial.println("[INFO] Initializing SPI mux...");
  hat_spi_mux_begin();
  delay(100);

  Serial.println("[INFO] Initializing ST7701S...");
  ST7701S_Initial();
  Serial.println("[INFO] ST7701S initialization complete.");
  delay(2000);

  test_display_on_off();
  test_inversion();
  test_rotation();
  //test_color_mode();
  test_sleep_mode();

  Serial.println();
  Serial.println("[INFO] One-time test sequence complete.");
  Serial.println("[INFO] Repeating simple display toggle in loop.");
}

void loop()
{
  Serial.println("[LOOP] Blinking LCD CS...");
  blink_LCD_CS();
  delay(1000);
  
  ST7701S_DisplayOff();
  Serial.println("[LOOP] Display OFF");
  delay(1000);

  ST7701S_DisplayOn();
  Serial.println("[LOOP] Display ON");
  delay(1000);

  ST7701S_InversionOff();
  Serial.println("[LOOP] Inversion OFF");
  delay(1000);

  ST7701S_InversionOn();
  Serial.println("[LOOP] Inversion ON");
  delay(1000);
}