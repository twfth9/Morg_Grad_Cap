#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  HAT_LCD_COLOR_MODE_RGB565 = 0,
  HAT_LCD_COLOR_MODE_RGB666 = 1
} hat_lcd_color_mode_t;

typedef enum {
  HAT_LCD_CHANNEL_ORDER_RGB = 0,
  HAT_LCD_CHANNEL_ORDER_GRB = 1
} hat_lcd_channel_order_t;

typedef enum {
  HAT_LCD_SERIAL_MODE_9BIT  = 0,
  HAT_LCD_SERIAL_MODE_16BIT = 1
} hat_lcd_serial_mode_t;

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

  hat_lcd_color_mode_t color_mode;
  hat_lcd_channel_order_t channel_order;
  hat_lcd_serial_mode_t serial_mode;
} hat_lcd_config_t;

/* -----------------------------
   High-level bring-up
   ----------------------------- */

void hat_lcd_init(void);
bool hat_lcd_begin(void);
bool hat_lcd_begin_verbose(bool verbose);
bool hat_lcd_begin_with_config(const hat_lcd_config_t *cfg);

/* -----------------------------
   Config access
   ----------------------------- */

void hat_lcd_get_default_config(hat_lcd_config_t *cfg);
void hat_lcd_set_config(const hat_lcd_config_t *cfg);
void hat_lcd_set_verbose(bool verbose);

/* -----------------------------
   Optional config overrides
   ----------------------------- */

void hat_lcd_set_resolution(uint16_t h_res, uint16_t v_res);
void hat_lcd_set_pixel_clock(uint32_t pclk_hz);
void hat_lcd_set_h_timing(uint16_t pulse, uint16_t back_porch, uint16_t front_porch);
void hat_lcd_set_v_timing(uint16_t pulse, uint16_t back_porch, uint16_t front_porch);
void hat_lcd_set_pclk_active_neg(bool active_neg);
void hat_lcd_set_color_mode(hat_lcd_color_mode_t mode);
void hat_lcd_set_channel_order(hat_lcd_channel_order_t order);
void hat_lcd_set_serial_mode(hat_lcd_serial_mode_t mode);

/* -----------------------------
   Status
   ----------------------------- */

bool hat_lcd_is_initialized(void);
uint16_t hat_lcd_width(void);
uint16_t hat_lcd_height(void);
uint16_t *hat_lcd_framebuffer(void);

/* -----------------------------
   Panel control
   ----------------------------- */

void hat_lcd_display_on(void);
void hat_lcd_display_off(void);
void hat_lcd_inversion_on(void);
void hat_lcd_inversion_off(void);

void hat_lcd_all_pixels_on(void);
void hat_lcd_all_pixels_off(void);
void hat_lcd_normal_display_on(void);

/* -----------------------------
   Color helpers
   ----------------------------- */

uint16_t hat_lcd_rgb565(uint8_t r, uint8_t g, uint8_t b);
uint16_t hat_lcd_color(uint8_t c0, uint8_t c1, uint8_t c2);

/* -----------------------------
   Drawing helpers
   ----------------------------- */

void hat_lcd_fill_screen(uint16_t color);
void hat_lcd_show_color(uint16_t color);
void hat_lcd_clear(void);

void hat_lcd_draw_pixel(int16_t x, int16_t y, uint16_t color);
void hat_lcd_draw_hline(int16_t x, int16_t y, int16_t w, uint16_t color);
void hat_lcd_draw_vline(int16_t x, int16_t y, int16_t h, uint16_t color);
void hat_lcd_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

void hat_lcd_draw_bitmap_rgb565(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *pixels);
void hat_lcd_draw_bitmap_rgb565_scaled(int16_t x, int16_t y,
                                       int16_t src_w, int16_t src_h,
                                       const uint16_t *pixels,
                                       int16_t dst_w, int16_t dst_h);

void hat_lcd_draw_vertical_bars(void);
void hat_lcd_draw_horizontal_bars(void);
void hat_lcd_draw_checkerboard(uint16_t color_a, uint16_t color_b, uint16_t cell_size);
void hat_lcd_draw_gradient(void);

/* -----------------------------
   Demo / diagnostic helpers
   ----------------------------- */

void hat_lcd_show_reference_colors(uint32_t dwell_ms);

#ifdef __cplusplus
}
#endif