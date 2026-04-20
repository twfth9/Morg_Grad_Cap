/*
 * File: HatLCD.h
 * Description: High-level LCD driver interface for the hat display. Exposes panel configuration, status, panel control, drawing helpers, and simple diagnostic/demo routines.
 * Function count: 31 public API functions and 4 public type definitions
 * Target microcontroller: ESP32
 */

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

/* Initialize the LCD subsystem using the current stored configuration.
   Input: None.
   Output: None. */
void hat_lcd_init(void);
/* Initialize the LCD using the current configuration with normal verbosity.
   Input: None.
   Output: true on success, false on failure. */
bool hat_lcd_begin(void);
/* Initialize the LCD and override the verbose logging setting for this startup.
   Input: Verbose-enable flag.
   Output: true on success, false on failure. */
bool hat_lcd_begin_verbose(bool verbose);
/* Initialize the LCD using a caller-supplied configuration structure.
   Input: Pointer to configuration structure.
   Output: true on success, false on failure. */
bool hat_lcd_begin_with_config(const hat_lcd_config_t *cfg);

/* -----------------------------
   Config access
   ----------------------------- */

/* Fill a caller-supplied structure with the default LCD timing and mode values.
   Input: Pointer to configuration structure.
   Output: None. */
void hat_lcd_get_default_config(hat_lcd_config_t *cfg);
/* Copy a full configuration structure into the driver state for later startup.
   Input: Pointer to configuration structure.
   Output: None. */
void hat_lcd_set_config(const hat_lcd_config_t *cfg);
/* Enable or disable verbose LCD debug logging.
   Input: Verbose-enable flag.
   Output: None. */
void hat_lcd_set_verbose(bool verbose);

/* -----------------------------
   Optional config overrides
   ----------------------------- */

/* Update the stored horizontal and vertical resolution.
   Input: Horizontal and vertical pixel counts.
   Output: None. */
void hat_lcd_set_resolution(uint16_t h_res, uint16_t v_res);
/* Update the stored RGB pixel-clock frequency.
   Input: Pixel clock in hertz.
   Output: None. */
void hat_lcd_set_pixel_clock(uint32_t pclk_hz);
/* Update horizontal sync timing parameters.
   Input: Pulse width, back porch, and front porch values.
   Output: None. */
void hat_lcd_set_h_timing(uint16_t pulse, uint16_t back_porch, uint16_t front_porch);
/* Update vertical sync timing parameters.
   Input: Pulse width, back porch, and front porch values.
   Output: None. */
void hat_lcd_set_v_timing(uint16_t pulse, uint16_t back_porch, uint16_t front_porch);
/* Set whether the RGB pixel clock is sampled on the negative edge.
   Input: Boolean edge-selection flag.
   Output: None. */
void hat_lcd_set_pclk_active_neg(bool active_neg);
/* Select the panel color transport mode used during initialization.
   Input: Color mode enum value.
   Output: None. */
void hat_lcd_set_color_mode(hat_lcd_color_mode_t mode);
/* Select whether incoming color data is interpreted as RGB or GRB ordered.
   Input: Channel order enum value.
   Output: None. */
void hat_lcd_set_channel_order(hat_lcd_channel_order_t order);
/* Select the serial sideband command transport width used during panel initialization.
   Input: Serial mode enum value.
   Output: None. */
void hat_lcd_set_serial_mode(hat_lcd_serial_mode_t mode);

/* -----------------------------
   Status
   ----------------------------- */

/* Report whether the LCD has already been initialized.
   Input: None.
   Output: true if initialized, otherwise false. */
bool hat_lcd_is_initialized(void);
/* Return the configured framebuffer width in pixels.
   Input: None.
   Output: Horizontal pixel count. */
uint16_t hat_lcd_width(void);
/* Return the configured framebuffer height in pixels.
   Input: None.
   Output: Vertical pixel count. */
uint16_t hat_lcd_height(void);
/* Return a pointer to the active RGB565 framebuffer, if available.
   Input: None.
   Output: Framebuffer pointer, or nullptr if unavailable. */
uint16_t *hat_lcd_framebuffer(void);

/* -----------------------------
   Panel control
   ----------------------------- */

/* Send the panel display-on command.
   Input: None.
   Output: None. */
void hat_lcd_display_on(void);
/* Send the panel display-off command.
   Input: None.
   Output: None. */
void hat_lcd_display_off(void);
/* Enable panel inversion mode.
   Input: None.
   Output: None. */
void hat_lcd_inversion_on(void);
/* Disable panel inversion mode.
   Input: None.
   Output: None. */
void hat_lcd_inversion_off(void);

/* Force the panel into all-pixels-on test mode.
   Input: None.
   Output: None. */
void hat_lcd_all_pixels_on(void);
/* Force the panel into all-pixels-off test mode.
   Input: None.
   Output: None. */
void hat_lcd_all_pixels_off(void);
/* Return the panel from all-pixels-on/off test mode to normal display mode.
   Input: None.
   Output: None. */
void hat_lcd_normal_display_on(void);

/* -----------------------------
   Color helpers
   ----------------------------- */

/* Pack 8-bit RGB channel values into a 16-bit RGB565 color word.
   Input: Red, green, and blue channel values.
   Output: Packed RGB565 color. */
uint16_t hat_lcd_rgb565(uint8_t r, uint8_t g, uint8_t b);
/* Pack 8-bit channel values into a color word using the currently selected channel ordering.
   Input: Three channel-byte values.
   Output: Packed RGB565 color. */
uint16_t hat_lcd_color(uint8_t c0, uint8_t c1, uint8_t c2);

/* -----------------------------
   Drawing helpers
   ----------------------------- */

/* Fill the entire framebuffer with one color.
   Input: Packed RGB565 color.
   Output: None. */
void hat_lcd_fill_screen(uint16_t color);
/* Alias/helper that fills the screen with a single color.
   Input: Packed RGB565 color.
   Output: None. */
void hat_lcd_show_color(uint16_t color);
/* Clear the framebuffer to black.
   Input: None.
   Output: None. */
void hat_lcd_clear(void);

/* Draw one pixel if the coordinates are inside the framebuffer bounds.
   Input: X, Y, and packed RGB565 color.
   Output: None. */
void hat_lcd_draw_pixel(int16_t x, int16_t y, uint16_t color);
/* Draw a horizontal line segment.
   Input: Start X, Y, width, and packed RGB565 color.
   Output: None. */
void hat_lcd_draw_hline(int16_t x, int16_t y, int16_t w, uint16_t color);
/* Draw a vertical line segment.
   Input: X, start Y, height, and packed RGB565 color.
   Output: None. */
void hat_lcd_draw_vline(int16_t x, int16_t y, int16_t h, uint16_t color);
/* Fill a rectangular region in the framebuffer.
   Input: X, Y, width, height, and packed RGB565 color.
   Output: None. */
void hat_lcd_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/* Blit an RGB565 bitmap into the framebuffer without scaling.
   Input: Destination X/Y, source width/height, and source pixel pointer.
   Output: None. */
void hat_lcd_draw_bitmap_rgb565(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *pixels);
/* Blit an RGB565 bitmap into the framebuffer with nearest-neighbor scaling.
   Input: Destination X/Y, source size, source pixel pointer, and destination size.
   Output: None. */
void hat_lcd_draw_bitmap_rgb565_scaled(int16_t x, int16_t y,
                                       int16_t src_w, int16_t src_h,
                                       const uint16_t *pixels,
                                       int16_t dst_w, int16_t dst_h);

/* Draw a simple vertical color-bar test pattern.
   Input: None.
   Output: None. */
void hat_lcd_draw_vertical_bars(void);
/* Draw a simple horizontal color-bar test pattern.
   Input: None.
   Output: None. */
void hat_lcd_draw_horizontal_bars(void);
/* Draw a checkerboard test pattern.
   Input: Two colors and checker cell size in pixels.
   Output: None. */
void hat_lcd_draw_checkerboard(uint16_t color_a, uint16_t color_b, uint16_t cell_size);
/* Draw a gradient test pattern across the framebuffer.
   Input: None.
   Output: None. */
void hat_lcd_draw_gradient(void);

/* -----------------------------
   Demo / diagnostic helpers
   ----------------------------- */

/* Cycle through reference colors for a fixed dwell time per color.
   Input: Dwell time in milliseconds.
   Output: None. */
void hat_lcd_show_reference_colors(uint32_t dwell_ms);

#ifdef __cplusplus
}
#endif
