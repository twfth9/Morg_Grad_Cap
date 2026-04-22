#include <Arduino.h>

extern "C" {
  #include "esp_lcd_panel_rgb.h"
  #include "esp_lcd_panel_ops.h"
  #include "esp_err.h"
}

#include "HatPins.h"
#include "HatSpiMux.h"
#include "LCD.h"
#include "Debug485.h"

static esp_lcd_panel_handle_t g_rgb_panel = nullptr;
static uint16_t *g_fb = nullptr;

static constexpr uint16_t LCD_H_RES = 480;
static constexpr uint16_t LCD_V_RES = 480;

/*
  First-pass trial timings.
  The module datasheet gives resolution and interface pins, but not a full porch table.
*/
static constexpr uint32_t LCD_PCLK_HZ = 12000000;

static constexpr uint16_t LCD_HSYNC_PULSE = 10;
static constexpr uint16_t LCD_HSYNC_BACK  = 40;
static constexpr uint16_t LCD_HSYNC_FRONT = 20;

static constexpr uint16_t LCD_VSYNC_PULSE = 4;
static constexpr uint16_t LCD_VSYNC_BACK  = 8;
static constexpr uint16_t LCD_VSYNC_FRONT = 8;

static void print_banner()
{
  Debug485.println();
  Debug485.println("========================================");
  Debug485.println("   ESP32-S3 ST7701S RGB565 LCD TEST");
  Debug485.println("========================================");
  Debug485.println();
}

static void print_lcd_pins()
{
  Debug485.println("LCD RGB pin map:");
  Debug485.printf("  PIN_LCD_PCLK  = %d\n", PIN_LCD_PCLK);
  Debug485.printf("  PIN_LCD_DE    = %d\n", PIN_LCD_DE);
  Debug485.printf("  PIN_LCD_VSYNC = %d\n", PIN_LCD_VSYNC);
  Debug485.printf("  PIN_LCD_HSYNC = %d\n", PIN_LCD_HSYNC);

  Debug485.printf("  PIN_LCD_DB0   = %d\n", PIN_LCD_DB0);
  Debug485.printf("  PIN_LCD_DB1   = %d\n", PIN_LCD_DB1);
  Debug485.printf("  PIN_LCD_DB2   = %d\n", PIN_LCD_DB2);
  Debug485.printf("  PIN_LCD_DB3   = %d\n", PIN_LCD_DB3);
  Debug485.printf("  PIN_LCD_DB4   = %d\n", PIN_LCD_DB4);
  Debug485.printf("  PIN_LCD_DB5   = %d\n", PIN_LCD_DB5);
  Debug485.printf("  PIN_LCD_DB6   = %d\n", PIN_LCD_DB6);
  Debug485.printf("  PIN_LCD_DB7   = %d\n", PIN_LCD_DB7);
  Debug485.printf("  PIN_LCD_DB8   = %d\n", PIN_LCD_DB8);
  Debug485.printf("  PIN_LCD_DB9   = %d\n", PIN_LCD_DB9);
  Debug485.printf("  PIN_LCD_DB10  = %d\n", PIN_LCD_DB10);
  Debug485.printf("  PIN_LCD_DB11  = %d\n", PIN_LCD_DB11);
  Debug485.printf("  PIN_LCD_DB12  = %d\n", PIN_LCD_DB12);
  Debug485.printf("  PIN_LCD_DB13  = %d\n", PIN_LCD_DB13);
  Debug485.printf("  PIN_LCD_DB14  = %d\n", PIN_LCD_DB14);
  Debug485.printf("  PIN_LCD_DB15  = %d\n", PIN_LCD_DB15);
  Debug485.println();
}

static bool init_rgb_panel(bool pclk_active_neg)
{
  static int lcd_data_gpios[16] = {
    PIN_LCD_DB0,  PIN_LCD_DB1,  PIN_LCD_DB2,  PIN_LCD_DB3,
    PIN_LCD_DB4,  PIN_LCD_DB5,  PIN_LCD_DB6,  PIN_LCD_DB7,
    PIN_LCD_DB8,  PIN_LCD_DB9,  PIN_LCD_DB10, PIN_LCD_DB11,
    PIN_LCD_DB12, PIN_LCD_DB13, PIN_LCD_DB14, PIN_LCD_DB15
  };

  Debug485.printf("Free heap before RGB init: %u\n", ESP.getFreeHeap());
  Debug485.printf("Total PSRAM: %u\n", ESP.getPsramSize());
  Debug485.printf("Free PSRAM before RGB init: %u\n", ESP.getFreePsram());

  esp_lcd_rgb_panel_config_t panel_config = {};
  panel_config.data_width = 16;
  panel_config.clk_src = LCD_CLK_SRC_DEFAULT;

  /*
    Let the driver infer 16bpp from data_width.
    This matches Espressif's documented behavior.
  */
  panel_config.bits_per_pixel = 0;

  /*
    Official PSRAM-backed mode:
    main framebuffer in PSRAM
    small bounce buffers in internal RAM
  */
  panel_config.bounce_buffer_size_px = 10 * LCD_H_RES;

  panel_config.disp_gpio_num  = -1;
  panel_config.pclk_gpio_num  = PIN_LCD_PCLK;
  panel_config.vsync_gpio_num = PIN_LCD_VSYNC;
  panel_config.hsync_gpio_num = PIN_LCD_HSYNC;
  panel_config.de_gpio_num    = PIN_LCD_DE;

  for (int i = 0; i < 16; ++i) {
    panel_config.data_gpio_nums[i] = lcd_data_gpios[i];
  }

  panel_config.timings.pclk_hz = LCD_PCLK_HZ;
  panel_config.timings.h_res = LCD_H_RES;
  panel_config.timings.v_res = LCD_V_RES;
  panel_config.timings.hsync_back_porch  = LCD_HSYNC_BACK;
  panel_config.timings.hsync_front_porch = LCD_HSYNC_FRONT;
  panel_config.timings.hsync_pulse_width = LCD_HSYNC_PULSE;
  panel_config.timings.vsync_back_porch  = LCD_VSYNC_BACK;
  panel_config.timings.vsync_front_porch = LCD_VSYNC_FRONT;
  panel_config.timings.vsync_pulse_width = LCD_VSYNC_PULSE;
  panel_config.timings.flags.pclk_active_neg = pclk_active_neg ? 1 : 0;

  panel_config.flags.fb_in_psram = 1;

  esp_err_t err = esp_lcd_new_rgb_panel(&panel_config, &g_rgb_panel);
  Debug485.printf("esp_lcd_new_rgb_panel: %s (%d)\n", esp_err_to_name(err), (int)err);
  if (err != ESP_OK) return false;

  err = esp_lcd_panel_reset(g_rgb_panel);
  Debug485.printf("esp_lcd_panel_reset: %s (%d)\n", esp_err_to_name(err), (int)err);
  if (err != ESP_OK) return false;

  err = esp_lcd_panel_init(g_rgb_panel);
  Debug485.printf("esp_lcd_panel_init: %s (%d)\n", esp_err_to_name(err), (int)err);
  if (err != ESP_OK) return false;

  err = esp_lcd_rgb_panel_get_frame_buffer(g_rgb_panel, 1, (void **)&g_fb);
  Debug485.printf("esp_lcd_rgb_panel_get_frame_buffer: %s (%d)\n", esp_err_to_name(err), (int)err);
  if (err != ESP_OK || g_fb == nullptr) return false;

  Debug485.printf("Framebuffer ptr: %p\n", g_fb);
  return true;
}

static void fill_screen(uint16_t color)
{
  const size_t pixel_count = (size_t)LCD_H_RES * (size_t)LCD_V_RES;
  for (size_t i = 0; i < pixel_count; ++i) {
    g_fb[i] = color;
  }
}

static void show_color(const char *name, uint16_t color)
{
  Debug485.printf("Showing %s (0x%04X)\n", name, color);
  fill_screen(color);
}

void setup()
{
  Debug485.begin(115200);
  delay(1000);

  print_banner();
  print_lcd_pins();

  Debug485.println("Initializing LCD SPI mux...");
  hat_spi_mux_begin();
  delay(20);

  Debug485.println("Initializing ST7701S over SPI...");
  ST7701S_Initial();

  /*
    Be explicit about how the ST7701S expects incoming RGB data.
    Start with:
      RGB565
      DE mode
      VSYNC active low
      HSYNC active low
      sample on positive PCLK edge
      write when DE = high
  */
  ST7701S_SetColorMode(ST7701_COLOR_MODE_RGB565);
  ST7701S_SetRGBInterfaceDEMode(false, false, false, false);

  Debug485.println("Initializing ESP32-S3 RGB panel peripheral...");
  Debug485.printf("Boot PSRAM size: %u\n", ESP.getPsramSize());
  while (!init_rgb_panel(false)) {
    Debug485.println("RGB init failed with pclk_active_neg = false");
    lcd_select();
    delay(250);
    lcd_deselect();
    delay(250);
  }

  Debug485.println("RGB peripheral initialized.");
  delay(100);

  show_color("RED",   0xF800);
  delay(2000);
  show_color("GREEN", 0x07E0);
  delay(2000);
  show_color("BLUE",  0x001F);
  delay(2000);
  show_color("WHITE", 0xFFFF);
  delay(2000);
  show_color("BLACK", 0x0000);
  delay(2000);

  Debug485.println("Entering repeating color cycle...");
}

void loop()
{
  show_color("RED",   0xF800);
    lcd_select();
    delay(500);
    lcd_deselect();
    delay(500);
    lcd_select();
    delay(500);
    lcd_deselect();
    delay(500);

  show_color("GREEN", 0x07E0);
    lcd_select();
    delay(500);
    lcd_deselect();
    delay(500);
    lcd_select();
    delay(500);
    lcd_deselect();
    delay(500);

  show_color("BLUE",  0x001F);
    lcd_select();
    delay(500);
    lcd_deselect();
    delay(500);
    lcd_select();
    delay(500);
    lcd_deselect();
    delay(500);

  show_color("WHITE", 0xFFFF);
    lcd_select();
    delay(500);
    lcd_deselect();
    delay(500);
    lcd_select();
    delay(500);
    lcd_deselect();
    delay(500);

  show_color("BLACK", 0x0000);
    lcd_select();
    delay(500);
    lcd_deselect();
    delay(500);
    lcd_select();
    delay(500);
    lcd_deselect();
    delay(500);
}