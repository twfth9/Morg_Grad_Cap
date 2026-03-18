#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  LCD_COLOR_MODE_RGB565 = 0,
  LCD_COLOR_MODE_RGB666 = 1
} lcd_color_mode_t;

typedef enum {
  LCD_CHANNEL_ORDER_RGB = 0,
  LCD_CHANNEL_ORDER_GRB = 1
} lcd_channel_order_t;

typedef struct {
  uint16_t h_res;
  uint16_t v_res;

  uint16_t hsync_pulse_width;
  uint16_t hsync_back_porch;
  uint16_t hsync_front_porch;

  uint16_t vsync_pulse_width;
  uint16_t vsync_back_porch;
  uint16_t vsync_front_porch;

  uint32_t pclk_hz;

  bool pclk_active_neg;
  bool verbose;

  lcd_color_mode_t color_mode;
  lcd_channel_order_t channel_order;
} lcd_config_t;

/* -----------------------------
   High-level bring-up
   ----------------------------- */

void lcd_init(void);
bool lcd_begin(void);
bool lcd_begin_with_config(const lcd_config_t *cfg);

/* -----------------------------
   Default config access
   ----------------------------- */

void lcd_get_default_config(lcd_config_t *cfg);
void lcd_set_config(const lcd_config_t *cfg);
void lcd_set_verbose(bool verbose);

/* -----------------------------
   Timing / mode helpers
   ----------------------------- */

void lcd_set_resolution(uint16_t h_res, uint16_t v_res);
void lcd_set_pixel_clock(uint32_t pclk_hz);
void lcd_set_h_timing(uint16_t pulse, uint16_t back_porch, uint16_t front_porch);
void lcd_set_v_timing(uint16_t pulse, uint16_t back_porch, uint16_t front_porch);
void lcd_set_pclk_active_neg(bool active_neg);
void lcd_set_color_mode(lcd_color_mode_t mode);

/* -----------------------------
   Channel order helpers
   ----------------------------- */

void lcd_set_channel_order(lcd_channel_order_t order);
lcd_channel_order_t lcd_get_channel_order(void);

/* -----------------------------
   Status
   ----------------------------- */

bool lcd_is_initialized(void);
uint16_t lcd_width(void);
uint16_t lcd_height(void);
uint16_t *lcd_framebuffer(void);

/* -----------------------------
   Panel control
   ----------------------------- */

void lcd_display_on(void);
void lcd_display_off(void);
void lcd_inversion_on(void);
void lcd_inversion_off(void);

/* -----------------------------
   Drawing helpers
   ----------------------------- */

void lcd_fill_screen(uint16_t color);
void lcd_show_color(uint16_t color);
void lcd_clear(void);

void lcd_draw_pixel(int16_t x, int16_t y, uint16_t color);
void lcd_draw_hline(int16_t x, int16_t y, int16_t w, uint16_t color);
void lcd_draw_vline(int16_t x, int16_t y, int16_t h, uint16_t color);
void lcd_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

void lcd_draw_bitmap_rgb565(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *pixels);
void lcd_draw_bitmap_rgb565_scaled(int16_t x, int16_t y,
                                   int16_t src_w, int16_t src_h,
                                   const uint16_t *pixels,
                                   int16_t dst_w, int16_t dst_h);

void lcd_draw_vertical_bars(void);
void lcd_draw_horizontal_bars(void);
void lcd_draw_checkerboard(uint16_t color_a, uint16_t color_b, uint16_t cell_size);
void lcd_draw_gradient(void);

/* -----------------------------
   Color helpers
   ----------------------------- */

uint16_t lcd_color(uint8_t c0, uint8_t c1, uint8_t c2);
uint16_t lcd_rgb565(uint8_t r, uint8_t g, uint8_t b);
uint16_t lcd_grb565(uint8_t g, uint8_t r, uint8_t b);

#ifdef __cplusplus
}
#endif
