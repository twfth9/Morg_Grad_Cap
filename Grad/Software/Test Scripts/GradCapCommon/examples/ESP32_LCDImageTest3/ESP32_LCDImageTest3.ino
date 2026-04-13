#include <Arduino.h>
#include <TJpg_Decoder.h>

#include "HatLCD.h"
#include "Debug485.h"
#include "pillars_jpg.h"

static bool jpg_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  uint16_t *fb = hat_lcd_framebuffer();
  if (!fb) return false;

  const int16_t screen_w = (int16_t)hat_lcd_width();
  const int16_t screen_h = (int16_t)hat_lcd_height();

  if (x >= screen_w || y >= screen_h) return false;
  if (x + (int16_t)w <= 0 || y + (int16_t)h <= 0) return true;

  int16_t src_x0 = 0;
  int16_t src_y0 = 0;
  int16_t draw_w = (int16_t)w;
  int16_t draw_h = (int16_t)h;
  int16_t dst_x = x;
  int16_t dst_y = y;

  if (dst_x < 0) {
    src_x0 = -dst_x;
    draw_w -= src_x0;
    dst_x = 0;
  }
  if (dst_y < 0) {
    src_y0 = -dst_y;
    draw_h -= src_y0;
    dst_y = 0;
  }
  if (dst_x + draw_w > screen_w) {
    draw_w = screen_w - dst_x;
  }
  if (dst_y + draw_h > screen_h) {
    draw_h = screen_h - dst_y;
  }

  if (draw_w <= 0 || draw_h <= 0) return true;

  for (int16_t row = 0; row < draw_h; ++row) {
    uint16_t *dst = fb + ((size_t)(dst_y + row) * (size_t)screen_w) + (size_t)dst_x;
    uint16_t *src = bitmap + ((size_t)(src_y0 + row) * (size_t)w) + (size_t)src_x0;
    memcpy(dst, src, (size_t)draw_w * sizeof(uint16_t));
  }

  return true;
}

void setup()
{
  Debug485.begin(115200);
  Debug485.println();
  Debug485.println("[JPEG_TEST] Starting.");

  if (!hat_lcd_begin_verbose(true)) {
    Debug485.println("[JPEG_TEST] hat_lcd_begin_verbose() failed.");
    while (1) delay(1000);
  }

  hat_lcd_clear();

  TJpgDec.setCallback(jpg_output);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);

  uint16_t jpg_w = 0;
  uint16_t jpg_h = 0;
  JRESULT size_result = TJpgDec.getJpgSize(&jpg_w, &jpg_h, pillars_jpg, pillars_jpg_len);

  Debug485.printf("[JPEG_TEST] JPEG size result=%d, w=%u, h=%u, bytes=%lu\n",
                  (int)size_result, jpg_w, jpg_h, (unsigned long)pillars_jpg_len);

  if (size_result != JDR_OK) {
    Debug485.println("[JPEG_TEST] Failed to read JPEG header.");
    while (1) delay(1000);
  }

  int16_t x = ((int16_t)hat_lcd_width()  - (int16_t)jpg_w) / 2;
  int16_t y = ((int16_t)hat_lcd_height() - (int16_t)jpg_h) / 2;

  uint32_t t0 = millis();
  JRESULT draw_result = TJpgDec.drawJpg(x, y, pillars_jpg, pillars_jpg_len);
  uint32_t dt = millis() - t0;

  Debug485.printf("[JPEG_TEST] draw result=%d, decode time=%lu ms\n",
                  (int)draw_result, (unsigned long)dt);

  if (draw_result != JDR_OK) {
    Debug485.println("[JPEG_TEST] JPEG draw failed.");
  } else {
    Debug485.println("[JPEG_TEST] JPEG draw complete.");
  }
}

void loop()
{
  delay(1000);
}
