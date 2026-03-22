#include "LCD.h"

#include <stdarg.h>
#include <string.h>

extern "C" {
  #include "esp_lcd_panel_rgb.h"
  #include "esp_lcd_panel_ops.h"
  #include "esp_err.h"
}

#include "HatPins.h"
#include "HatSpiMux.h"
#include "Debug485.h"

/* =========================================================
   Private state
   ========================================================= */

static esp_lcd_panel_handle_t g_rgb_panel = nullptr;
static uint16_t *g_fb = nullptr;
static bool g_lcd_initialized = false;

static lcd_config_t g_cfg = {
  .h_res = 480,
  .v_res = 480,

  .hsync_pulse_width = 10,
  .hsync_back_porch  = 40,
  .hsync_front_porch = 20,

  .vsync_pulse_width = 4,
  .vsync_back_porch  = 8,
  .vsync_front_porch = 8,

  .pclk_hz = 12000000,

  .pclk_active_neg = false,
  .verbose = false,

  .color_mode = LCD_COLOR_MODE_RGB565,
  .channel_order = LCD_CHANNEL_ORDER_GRB
};

static int g_lcd_data_gpios[16] = {
  PIN_LCD_DB0,  PIN_LCD_DB1,  PIN_LCD_DB2,  PIN_LCD_DB3,
  PIN_LCD_DB4,  PIN_LCD_DB5,  PIN_LCD_DB6,  PIN_LCD_DB7,
  PIN_LCD_DB8,  PIN_LCD_DB9,  PIN_LCD_DB10, PIN_LCD_DB11,
  PIN_LCD_DB12, PIN_LCD_DB13, PIN_LCD_DB14, PIN_LCD_DB15
};

/* =========================================================
   Debug helpers
   ========================================================= */

static void lcd_log(const char *msg)
{
  if (g_cfg.verbose) {
    Debug485.println(msg);
  }
}

static void lcd_logf(const char *fmt, ...)
{
  if (!g_cfg.verbose) return;

  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Debug485.print(buf);
}

/* =========================================================
   ST7701S low-level 9-bit SPI sideband
   ========================================================= */

static inline void lcd_spi_delay(void)
{
  delayMicroseconds(1);
}

static inline void lcd_bus_begin(void)
{
  pinMode(PIN_SPI_MOSI, OUTPUT);
  pinMode(PIN_SPI_SCK, OUTPUT);

  digitalWrite(PIN_SPI_MOSI, LOW);
  digitalWrite(PIN_SPI_SCK, LOW);
}

static inline void lcd_select(void)
{
  hat_spi_mux_select_lcd();
}

static inline void lcd_deselect(void)
{
  hat_spi_mux_disconnect();
}

static inline void lcd_write_9bit(bool is_data, uint8_t value)
{
  lcd_select();

  digitalWrite(PIN_SPI_SCK, LOW);
  digitalWrite(PIN_SPI_MOSI, is_data ? HIGH : LOW);
  lcd_spi_delay();
  digitalWrite(PIN_SPI_SCK, HIGH);
  lcd_spi_delay();

  for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
    digitalWrite(PIN_SPI_SCK, LOW);
    digitalWrite(PIN_SPI_MOSI, (value & mask) ? HIGH : LOW);
    lcd_spi_delay();
    digitalWrite(PIN_SPI_SCK, HIGH);
    lcd_spi_delay();
  }

  digitalWrite(PIN_SPI_SCK, LOW);
  lcd_deselect();
}

static inline void st7701_write_command(uint8_t cmd)
{
  lcd_write_9bit(false, cmd);
}

static inline void st7701_write_data(uint8_t data)
{
  lcd_write_9bit(true, data);
}

static inline void st7701_write_register1(uint8_t cmd, uint8_t d0)
{
  st7701_write_command(cmd);
  st7701_write_data(d0);
}

static inline void st7701_write_register2(uint8_t cmd, uint8_t d0, uint8_t d1)
{
  st7701_write_command(cmd);
  st7701_write_data(d0);
  st7701_write_data(d1);
}

static inline void st7701_write_register3(uint8_t cmd, uint8_t d0, uint8_t d1, uint8_t d2)
{
  st7701_write_command(cmd);
  st7701_write_data(d0);
  st7701_write_data(d1);
  st7701_write_data(d2);
}

static inline void st7701_write_bytes(uint8_t cmd, const uint8_t *data, size_t len)
{
  st7701_write_command(cmd);
  for (size_t i = 0; i < len; ++i) {
    st7701_write_data(data[i]);
  }
}

static inline void st7701_select_bank(uint8_t bank)
{
  st7701_write_command(0xFF);
  st7701_write_data(0x77);
  st7701_write_data(0x01);
  st7701_write_data(0x00);
  st7701_write_data(0x00);
  st7701_write_data(bank);
}

static inline void st7701_select_bank0(void)      { st7701_select_bank(0x10); }
static inline void st7701_select_bank1(void)      { st7701_select_bank(0x11); }
static inline void st7701_select_bank3(void)      { st7701_select_bank(0x13); }
static inline void st7701_select_normal_page(void){ st7701_select_bank(0x00); }

static inline void st7701_software_reset(void)
{
  st7701_write_command(0x01);
  delay(150);
}

static inline void st7701_sleep_out(void)
{
  st7701_write_command(0x11);
  delay(120);
}

static inline void st7701_display_on_cmd(void)
{
  st7701_write_command(0x29);
  delay(20);
}

static inline void st7701_display_off_cmd(void)
{
  st7701_write_command(0x28);
  delay(20);
}

static inline void st7701_inversion_on_cmd(void)
{
  st7701_write_command(0x21);
}

static inline void st7701_inversion_off_cmd(void)
{
  st7701_write_command(0x20);
}

static inline void st7701_set_madctl(uint8_t madctl)
{
  st7701_write_register1(0x36, madctl);
}

static inline void st7701_set_pixel_format(uint8_t colmod)
{
  st7701_write_register1(0x3A, colmod);
}

static void st7701_enable_rgb565(void)
{
  st7701_select_bank0();
  st7701_write_register1(0xCD, 0x00);
  st7701_select_normal_page();
  st7701_set_pixel_format(0x55);
}

static void st7701_enable_rgb666(void)
{
  st7701_select_bank0();
  st7701_write_register1(0xCD, 0x08);
  st7701_select_normal_page();
  st7701_set_pixel_format(0x66);
}

static void st7701_set_color_mode_internal(lcd_color_mode_t mode)
{
  if (mode == LCD_COLOR_MODE_RGB666) {
    st7701_enable_rgb666();
  } else {
    st7701_enable_rgb565();
  }
}

static void st7701_set_rgb_interface_de_mode(bool vsync_active_high,
                                             bool hsync_active_high,
                                             bool pclk_sample_negative_edge,
                                             bool de_active_low)
{
  uint8_t rgbctrl0 = 0x00; /* DE mode */

  if (vsync_active_high)         rgbctrl0 |= (1 << 3);
  if (hsync_active_high)         rgbctrl0 |= (1 << 2);
  if (pclk_sample_negative_edge) rgbctrl0 |= (1 << 1);
  if (de_active_low)             rgbctrl0 |= (1 << 0);

  st7701_select_bank0();
  st7701_write_register3(0xC3, rgbctrl0, 0x00, 0x00);
  st7701_select_normal_page();
}

static void st7701_init_sequence(void)
{
  lcd_log("Initializing ST7701S over SPI...");
  lcd_bus_begin();
  delay(120);
  st7701_software_reset();

  st7701_select_bank3();
  st7701_write_register1(0xEF, 0x08);

  st7701_select_bank0();
  st7701_write_register2(0xC0, 0x3B, 0x00);
  st7701_write_register2(0xC1, 0x0D, 0x02);
  st7701_write_register2(0xC2, 0x21, 0x08);

  {
    static const uint8_t data[] = {
      0x00,0x11,0x18,0x0E,0x11,0x06,0x07,0x08,
      0x07,0x22,0x04,0x12,0x0F,0xAA,0x31,0x18
    };
    st7701_write_bytes(0xB0, data, sizeof(data));
  }

  {
    static const uint8_t data[] = {
      0x00,0x11,0x19,0x0E,0x12,0x07,0x08,0x08,
      0x08,0x22,0x04,0x11,0x11,0xA9,0x32,0x18
    };
    st7701_write_bytes(0xB1, data, sizeof(data));
  }

  st7701_select_bank1();
  st7701_write_register1(0xB0, 0x60);
  st7701_write_register1(0xB1, 0x30);
  st7701_write_register1(0xB2, 0x87);
  st7701_write_register1(0xB3, 0x80);
  st7701_write_register1(0xB5, 0x49);
  st7701_write_register1(0xB7, 0x85);
  st7701_write_register1(0xB8, 0x21);
  st7701_write_register1(0xC1, 0x78);
  st7701_write_register1(0xC2, 0x78);

  delay(20);

  {
    static const uint8_t data[] = {0x00,0x1B,0x02};
    st7701_write_bytes(0xE0, data, sizeof(data));
  }
  {
    static const uint8_t data[] = {
      0x08,0xA0,0x00,0x00,0x07,0xA0,0x00,0x00,0x00,0x44,0x44
    };
    st7701_write_bytes(0xE1, data, sizeof(data));
  }
  {
    static const uint8_t data[] = {
      0x11,0x11,0x44,0x44,0xED,0xA0,0x00,0x00,0xEC,0xA0,0x00,0x00
    };
    st7701_write_bytes(0xE2, data, sizeof(data));
  }
  {
    static const uint8_t data[] = {0x00,0x00,0x11,0x11};
    st7701_write_bytes(0xE3, data, sizeof(data));
  }
  {
    static const uint8_t data[] = {0x44,0x44};
    st7701_write_bytes(0xE4, data, sizeof(data));
  }
  {
    static const uint8_t data[] = {
      0x0A,0xE9,0xD8,0xA0,0x0C,0xEB,0xD8,0xA0,
      0x0E,0xED,0xD8,0xA0,0x10,0xEF,0xD8,0xA0
    };
    st7701_write_bytes(0xE5, data, sizeof(data));
  }
  {
    static const uint8_t data[] = {0x00,0x00,0x11,0x11};
    st7701_write_bytes(0xE6, data, sizeof(data));
  }
  {
    static const uint8_t data[] = {0x44,0x44};
    st7701_write_bytes(0xE7, data, sizeof(data));
  }
  {
    static const uint8_t data[] = {
      0x09,0xE8,0xD8,0xA0,0x0B,0xEA,0xD8,0xA0,
      0x0D,0xEC,0xD8,0xA0,0x0F,0xEE,0xD8,0xA0
    };
    st7701_write_bytes(0xE8, data, sizeof(data));
  }
  {
    static const uint8_t data[] = {0x02,0x00,0xE4,0xE4,0x88,0x00,0x40};
    st7701_write_bytes(0xEB, data, sizeof(data));
  }
  {
    static const uint8_t data[] = {0x3C,0x00};
    st7701_write_bytes(0xEC, data, sizeof(data));
  }
  {
    static const uint8_t data[] = {
      0xAB,0x89,0x76,0x54,0x02,0xFF,0xFF,0xFF,
      0xFF,0xFF,0xFF,0x20,0x45,0x67,0x98,0xBA
    };
    st7701_write_bytes(0xED, data, sizeof(data));
  }
  {
    static const uint8_t data[] = {0x10,0x0D,0x04,0x08,0x3F,0x1F};
    st7701_write_bytes(0xEF, data, sizeof(data));
  }

  st7701_select_normal_page();
  st7701_set_color_mode_internal(g_cfg.color_mode);

  st7701_select_bank3();
  st7701_write_register2(0xE8, 0x00, 0x0E);
  st7701_select_normal_page();

  st7701_sleep_out();

  st7701_select_bank3();
  st7701_write_register2(0xE8, 0x00, 0x0C);
  delay(10);
  st7701_write_register2(0xE8, 0x00, 0x00);
  st7701_select_normal_page();

  st7701_set_madctl(0x00);
  st7701_set_color_mode_internal(g_cfg.color_mode);
  st7701_set_rgb_interface_de_mode(false, false, g_cfg.pclk_active_neg, false);
  st7701_inversion_on_cmd();
  st7701_display_on_cmd();
}

/* =========================================================
   RGB panel init
   ========================================================= */

static bool lcd_init_rgb_panel_internal(void)
{
  lcd_log("Initializing ESP32-S3 RGB panel peripheral...");
  lcd_logf("Boot PSRAM size: %u\n", ESP.getPsramSize());
  lcd_logf("Free heap before RGB init: %u\n", ESP.getFreeHeap());
  lcd_logf("Total PSRAM: %u\n", ESP.getPsramSize());
  lcd_logf("Free PSRAM before RGB init: %u\n", ESP.getFreePsram());

  esp_lcd_rgb_panel_config_t panel_config = {};
  panel_config.data_width = 16;
  panel_config.clk_src = LCD_CLK_SRC_DEFAULT;
  panel_config.bits_per_pixel = 0;
  panel_config.bounce_buffer_size_px = 10 * g_cfg.h_res;

  panel_config.disp_gpio_num  = -1;
  panel_config.pclk_gpio_num  = PIN_LCD_PCLK;
  panel_config.vsync_gpio_num = PIN_LCD_VSYNC;
  panel_config.hsync_gpio_num = PIN_LCD_HSYNC;
  panel_config.de_gpio_num    = PIN_LCD_DE;

  for (int i = 0; i < 16; ++i) {
    panel_config.data_gpio_nums[i] = g_lcd_data_gpios[i];
  }

  panel_config.timings.pclk_hz = g_cfg.pclk_hz;
  panel_config.timings.h_res = g_cfg.h_res;
  panel_config.timings.v_res = g_cfg.v_res;
  panel_config.timings.hsync_back_porch  = g_cfg.hsync_back_porch;
  panel_config.timings.hsync_front_porch = g_cfg.hsync_front_porch;
  panel_config.timings.hsync_pulse_width = g_cfg.hsync_pulse_width;
  panel_config.timings.vsync_back_porch  = g_cfg.vsync_back_porch;
  panel_config.timings.vsync_front_porch = g_cfg.vsync_front_porch;
  panel_config.timings.vsync_pulse_width = g_cfg.vsync_pulse_width;
  panel_config.timings.flags.pclk_active_neg = g_cfg.pclk_active_neg ? 1 : 0;

  panel_config.flags.fb_in_psram = 1;

  esp_err_t err = esp_lcd_new_rgb_panel(&panel_config, &g_rgb_panel);
  lcd_logf("esp_lcd_new_rgb_panel: %s (%d)\n", esp_err_to_name(err), (int)err);
  if (err != ESP_OK) return false;

  err = esp_lcd_panel_reset(g_rgb_panel);
  lcd_logf("esp_lcd_panel_reset: %s (%d)\n", esp_err_to_name(err), (int)err);
  if (err != ESP_OK) return false;

  err = esp_lcd_panel_init(g_rgb_panel);
  lcd_logf("esp_lcd_panel_init: %s (%d)\n", esp_err_to_name(err), (int)err);
  if (err != ESP_OK) return false;

  err = esp_lcd_rgb_panel_get_frame_buffer(g_rgb_panel, 1, (void **)&g_fb);
  lcd_logf("esp_lcd_rgb_panel_get_frame_buffer: %s (%d)\n", esp_err_to_name(err), (int)err);
  if (err != ESP_OK || g_fb == nullptr) return false;

  lcd_logf("Framebuffer ptr: %p\n", g_fb);
  return true;
}

/* =========================================================
   Public API
   ========================================================= */

void lcd_get_default_config(lcd_config_t *cfg)
{
  if (!cfg) return;
  *cfg = g_cfg;
}

void lcd_set_config(const lcd_config_t *cfg)
{
  if (!cfg) return;
  g_cfg = *cfg;
}

void lcd_set_verbose(bool verbose)
{
  g_cfg.verbose = verbose;
}

void lcd_set_resolution(uint16_t h_res, uint16_t v_res)
{
  g_cfg.h_res = h_res;
  g_cfg.v_res = v_res;
}

void lcd_set_pixel_clock(uint32_t pclk_hz)
{
  g_cfg.pclk_hz = pclk_hz;
}

void lcd_set_h_timing(uint16_t pulse, uint16_t back_porch, uint16_t front_porch)
{
  g_cfg.hsync_pulse_width = pulse;
  g_cfg.hsync_back_porch = back_porch;
  g_cfg.hsync_front_porch = front_porch;
}

void lcd_set_v_timing(uint16_t pulse, uint16_t back_porch, uint16_t front_porch)
{
  g_cfg.vsync_pulse_width = pulse;
  g_cfg.vsync_back_porch = back_porch;
  g_cfg.vsync_front_porch = front_porch;
}

void lcd_set_pclk_active_neg(bool active_neg)
{
  g_cfg.pclk_active_neg = active_neg;
}

void lcd_set_color_mode(lcd_color_mode_t mode)
{
  g_cfg.color_mode = mode;
}

void lcd_set_channel_order(lcd_channel_order_t order)
{
  g_cfg.channel_order = order;
}

lcd_channel_order_t lcd_get_channel_order(void)
{
  return g_cfg.channel_order;
}

bool lcd_is_initialized(void)
{
  return g_lcd_initialized;
}

uint16_t lcd_width(void)
{
  return g_cfg.h_res;
}

uint16_t lcd_height(void)
{
  return g_cfg.v_res;
}

uint16_t *lcd_framebuffer(void)
{
  return g_fb;
}

bool lcd_begin_with_config(const lcd_config_t *cfg)
{
  if (cfg) {
    g_cfg = *cfg;
  }

  hat_spi_mux_begin();
  st7701_init_sequence();

  if (!lcd_init_rgb_panel_internal()) {
    return false;
  }

  g_lcd_initialized = true;
  lcd_log("RGB peripheral initialized.");
  return true;
}

bool lcd_begin(void)
{
  return lcd_begin_with_config(nullptr);
}

void lcd_init(void)
{
  (void)lcd_begin();
}

void lcd_display_on(void)
{
  st7701_display_on_cmd();
}

void lcd_display_off(void)
{
  st7701_display_off_cmd();
}

void lcd_inversion_on(void)
{
  st7701_inversion_on_cmd();
}

void lcd_inversion_off(void)
{
  st7701_inversion_off_cmd();
}

uint16_t lcd_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return (uint16_t)(((r & 0xF8) << 8) |
                    ((g & 0xFC) << 3) |
                    ((b & 0xF8) >> 3));
}

uint16_t lcd_grb565(uint8_t g, uint8_t r, uint8_t b)
{
  return lcd_rgb565(r, g, b);
}

uint16_t lcd_color(uint8_t c0, uint8_t c1, uint8_t c2)
{
  if (g_cfg.channel_order == LCD_CHANNEL_ORDER_GRB) {
    return lcd_rgb565(c1, c0, c2);
  }
  return lcd_rgb565(c0, c1, c2);
}

void lcd_fill_screen(uint16_t color)
{
  if (!g_fb) return;

  const size_t pixel_count = (size_t)g_cfg.h_res * (size_t)g_cfg.v_res;
  for (size_t i = 0; i < pixel_count; ++i) {
    g_fb[i] = color;
  }
}

void lcd_show_color(uint16_t color)
{
  lcd_fill_screen(color);
}

void lcd_clear(void)
{
  lcd_fill_screen(0x0000);
}

void lcd_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
  if (!g_fb) return;
  if (x < 0 || y < 0) return;
  if (x >= (int16_t)g_cfg.h_res || y >= (int16_t)g_cfg.v_res) return;

  g_fb[(size_t)y * g_cfg.h_res + (size_t)x] = color;
}

void lcd_draw_hline(int16_t x, int16_t y, int16_t w, uint16_t color)
{
  if (!g_fb) return;
  if (y < 0 || y >= (int16_t)g_cfg.v_res || w <= 0) return;

  int16_t x0 = x;
  int16_t x1 = x + w - 1;

  if (x0 < 0) x0 = 0;
  if (x1 >= (int16_t)g_cfg.h_res) x1 = (int16_t)g_cfg.h_res - 1;
  if (x0 > x1) return;

  size_t row = (size_t)y * g_cfg.h_res;
  for (int16_t xx = x0; xx <= x1; ++xx) {
    g_fb[row + (size_t)xx] = color;
  }
}

void lcd_draw_vline(int16_t x, int16_t y, int16_t h, uint16_t color)
{
  if (!g_fb) return;
  if (x < 0 || x >= (int16_t)g_cfg.h_res || h <= 0) return;

  int16_t y0 = y;
  int16_t y1 = y + h - 1;

  if (y0 < 0) y0 = 0;
  if (y1 >= (int16_t)g_cfg.v_res) y1 = (int16_t)g_cfg.v_res - 1;
  if (y0 > y1) return;

  for (int16_t yy = y0; yy <= y1; ++yy) {
    g_fb[(size_t)yy * g_cfg.h_res + (size_t)x] = color;
  }
}

void lcd_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  if (!g_fb) return;
  if (w <= 0 || h <= 0) return;

  int16_t x0 = x;
  int16_t y0 = y;
  int16_t x1 = x + w - 1;
  int16_t y1 = y + h - 1;

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 >= (int16_t)g_cfg.h_res) x1 = (int16_t)g_cfg.h_res - 1;
  if (y1 >= (int16_t)g_cfg.v_res) y1 = (int16_t)g_cfg.v_res - 1;

  if (x0 > x1 || y0 > y1) return;

  for (int16_t yy = y0; yy <= y1; ++yy) {
    size_t row = (size_t)yy * g_cfg.h_res;
    for (int16_t xx = x0; xx <= x1; ++xx) {
      g_fb[row + (size_t)xx] = color;
    }
  }
}

void lcd_draw_bitmap_rgb565(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *pixels)
{
  if (!g_fb || !pixels) return;
  if (w <= 0 || h <= 0) return;

  for (int16_t yy = 0; yy < h; ++yy) {
    int16_t dy = y + yy;
    if (dy < 0 || dy >= (int16_t)g_cfg.v_res) continue;

    for (int16_t xx = 0; xx < w; ++xx) {
      int16_t dx = x + xx;
      if (dx < 0 || dx >= (int16_t)g_cfg.h_res) continue;

      g_fb[(size_t)dy * g_cfg.h_res + (size_t)dx] =
          pixels[(size_t)yy * (size_t)w + (size_t)xx];
    }
  }
}

void lcd_draw_bitmap_rgb565_scaled(int16_t x, int16_t y,
                                   int16_t src_w, int16_t src_h,
                                   const uint16_t *pixels,
                                   int16_t dst_w, int16_t dst_h)
{
  if (!g_fb || !pixels) return;
  if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;

  for (int16_t yy = 0; yy < dst_h; ++yy) {
    int16_t dy = y + yy;
    if (dy < 0 || dy >= (int16_t)g_cfg.v_res) continue;

    int16_t sy = (int32_t)yy * src_h / dst_h;

    for (int16_t xx = 0; xx < dst_w; ++xx) {
      int16_t dx = x + xx;
      if (dx < 0 || dx >= (int16_t)g_cfg.h_res) continue;

      int16_t sx = (int32_t)xx * src_w / dst_w;
      g_fb[(size_t)dy * g_cfg.h_res + (size_t)dx] =
          pixels[(size_t)sy * (size_t)src_w + (size_t)sx];
    }
  }
}

void lcd_draw_vertical_bars(void)
{
  if (!g_fb) return;

  const uint16_t colors[] = {
    lcd_color(255,   0,   0),
    lcd_color(  0, 255,   0),
    lcd_color(  0,   0, 255),
    lcd_color(255, 255, 255),
    lcd_color(255, 255,   0),
    lcd_color(255,   0, 255),
    lcd_color(  0, 255, 255),
    lcd_color(  0,   0,   0)
  };
  const uint16_t bar_count = (uint16_t)(sizeof(colors) / sizeof(colors[0]));
  const uint16_t bar_width = g_cfg.h_res / bar_count;

  for (uint16_t y = 0; y < g_cfg.v_res; ++y) {
    for (uint16_t x = 0; x < g_cfg.h_res; ++x) {
      uint16_t idx = x / bar_width;
      if (idx >= bar_count) idx = bar_count - 1;
      g_fb[(size_t)y * g_cfg.h_res + x] = colors[idx];
    }
  }
}

void lcd_draw_horizontal_bars(void)
{
  if (!g_fb) return;

  const uint16_t colors[] = {
    lcd_color(255,   0,   0),
    lcd_color(  0, 255,   0),
    lcd_color(  0,   0, 255),
    lcd_color(255, 255, 255),
    lcd_color(255, 255,   0),
    lcd_color(255,   0, 255),
    lcd_color(  0, 255, 255),
    lcd_color(  0,   0,   0)
  };
  const uint16_t bar_count = (uint16_t)(sizeof(colors) / sizeof(colors[0]));
  const uint16_t bar_height = g_cfg.v_res / bar_count;

  for (uint16_t y = 0; y < g_cfg.v_res; ++y) {
    uint16_t idx = y / bar_height;
    if (idx >= bar_count) idx = bar_count - 1;

    for (uint16_t x = 0; x < g_cfg.h_res; ++x) {
      g_fb[(size_t)y * g_cfg.h_res + x] = colors[idx];
    }
  }
}

void lcd_draw_checkerboard(uint16_t color_a, uint16_t color_b, uint16_t cell_size)
{
  if (!g_fb || cell_size == 0) return;

  for (uint16_t y = 0; y < g_cfg.v_res; ++y) {
    for (uint16_t x = 0; x < g_cfg.h_res; ++x) {
      bool sel = (((x / cell_size) + (y / cell_size)) & 1) != 0;
      g_fb[(size_t)y * g_cfg.h_res + x] = sel ? color_a : color_b;
    }
  }
}

void lcd_draw_gradient(void)
{
  if (!g_fb) return;

  for (uint16_t y = 0; y < g_cfg.v_res; ++y) {
    for (uint16_t x = 0; x < g_cfg.h_res; ++x) {
      uint8_t r = (uint8_t)((x * 255UL) / (g_cfg.h_res - 1));
      uint8_t g = (uint8_t)((y * 255UL) / (g_cfg.v_res - 1));
      uint8_t b = (uint8_t)(((x + y) * 255UL) / (g_cfg.h_res + g_cfg.v_res - 2));
      g_fb[(size_t)y * g_cfg.h_res + x] = lcd_color(r, g, b);
    }
  }
}
