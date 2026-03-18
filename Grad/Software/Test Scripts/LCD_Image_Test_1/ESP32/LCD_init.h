#ifndef LCD_INIT_H
#define LCD_INIT_H

#include <Arduino.h>
#include "HatPins.h"
#include "HatSpiMux.h"

/*
  ST7701S sideband SPI driver for grad cap LCD board

  Notes:
  - This configures the ST7701S over its 9-bit SPI sideband interface.
  - This does NOT draw pixels; the RGB parallel bus still must be driven separately.
  - LCD CS is selected through the HatSpiMux.
  - Because the panel RESET pin is not connected to the ESP32, software reset is used.

  9-bit SPI framing:
    first bit  = D/CX   (0 = command, 1 = data)
    next 8 bits = payload byte

  Bus wiring:
    MOSI -> PIN_SPI_MOSI
    SCK  -> PIN_SPI_SCK
*/

/* =========================
   ST7701S command constants
   ========================= */
#define ST7701_CMD_NOP         0x00
#define ST7701_CMD_SWRESET     0x01
#define ST7701_CMD_SLPIN       0x10
#define ST7701_CMD_SLPOUT      0x11
#define ST7701_CMD_INVOFF      0x20
#define ST7701_CMD_INVON       0x21
#define ST7701_CMD_DISPOFF     0x28
#define ST7701_CMD_DISPON      0x29
#define ST7701_CMD_MADCTL      0x36
#define ST7701_CMD_COLMOD      0x3A
#define ST7701_CMD_CND2BKxSEL  0xFF

/* MADCTL bits */
#define ST7701_MADCTL_MY   0x80
#define ST7701_MADCTL_MX   0x40
#define ST7701_MADCTL_MV   0x20
#define ST7701_MADCTL_ML   0x10
#define ST7701_MADCTL_BGR  0x08
#define ST7701_MADCTL_MH   0x04

/* COLMOD values */
#define ST7701_COLMOD_RGB565  0x55
#define ST7701_COLMOD_RGB666  0x66
#define ST7701_COLMOD_RGB888  0x77

/* Color mode selection for this project */
#define ST7701_COLOR_MODE_RGB565  0
#define ST7701_COLOR_MODE_RGB666  1

/* Default for your board: DB0..DB15 only */
#ifndef ST7701_DEFAULT_COLOR_MODE
#define ST7701_DEFAULT_COLOR_MODE ST7701_COLOR_MODE_RGB565
#endif

/* =========================
   Low-level 9-bit SPI
   ========================= */

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

/*
  Send one 9-bit frame:
    bit 8   = D/CX
    bit 7:0 = payload
*/
static inline void lcd_write_9bit(bool isData, uint8_t value)
{
  lcd_select();

  /* D/CX bit first */
  digitalWrite(PIN_SPI_SCK, LOW);
  digitalWrite(PIN_SPI_MOSI, isData ? HIGH : LOW);
  lcd_spi_delay();
  digitalWrite(PIN_SPI_SCK, HIGH);
  lcd_spi_delay();

  /* 8 payload bits, MSB first */
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

/* =========================
   Public write helpers
   ========================= */

static inline void ST7701S_WriteCommand(uint8_t cmd)
{
  lcd_write_9bit(false, cmd);
}

static inline void ST7701S_WriteData(uint8_t data)
{
  lcd_write_9bit(true, data);
}

static inline void ST7701S_WriteRegister1(uint8_t cmd, uint8_t d0)
{
  ST7701S_WriteCommand(cmd);
  ST7701S_WriteData(d0);
}

static inline void ST7701S_WriteRegister2(uint8_t cmd, uint8_t d0, uint8_t d1)
{
  ST7701S_WriteCommand(cmd);
  ST7701S_WriteData(d0);
  ST7701S_WriteData(d1);
}

static inline void ST7701S_WriteBytes(uint8_t cmd, const uint8_t *data, size_t len)
{
  ST7701S_WriteCommand(cmd);
  for (size_t i = 0; i < len; ++i) {
    ST7701S_WriteData(data[i]);
  }
}

/* =========================
   Command2 bank helpers
   ========================= */

static inline void ST7701S_SelectBank(uint8_t bank)
{
  ST7701S_WriteCommand(ST7701_CMD_CND2BKxSEL);
  ST7701S_WriteData(0x77);
  ST7701S_WriteData(0x01);
  ST7701S_WriteData(0x00);
  ST7701S_WriteData(0x00);
  ST7701S_WriteData(bank);
}

static inline void ST7701S_SelectBank0(void)      { ST7701S_SelectBank(0x10); }
static inline void ST7701S_SelectBank1(void)      { ST7701S_SelectBank(0x11); }
static inline void ST7701S_SelectBank3(void)      { ST7701S_SelectBank(0x13); }
static inline void ST7701S_SelectNormalPage(void) { ST7701S_SelectBank(0x00); }

/* =========================
   Diagnostic / control helpers
   ========================= */

static inline void ST7701S_SoftwareReset(void)
{
  ST7701S_WriteCommand(ST7701_CMD_SWRESET);
  delay(150);
}

static inline void ST7701S_SleepOut(void)
{
  ST7701S_WriteCommand(ST7701_CMD_SLPOUT);
  delay(120);
}

static inline void ST7701S_SleepIn(void)
{
  ST7701S_WriteCommand(ST7701_CMD_SLPIN);
  delay(120);
}

static inline void ST7701S_DisplayOn(void)
{
  ST7701S_WriteCommand(ST7701_CMD_DISPON);
  delay(20);
}

static inline void ST7701S_DisplayOff(void)
{
  ST7701S_WriteCommand(ST7701_CMD_DISPOFF);
  delay(20);
}

static inline void ST7701S_InversionOn(void)
{
  ST7701S_WriteCommand(ST7701_CMD_INVON);
}

static inline void ST7701S_InversionOff(void)
{
  ST7701S_WriteCommand(ST7701_CMD_INVOFF);
}

static inline void ST7701S_SetMADCTL(uint8_t madctl)
{
  ST7701S_WriteRegister1(ST7701_CMD_MADCTL, madctl);
}

static inline void ST7701S_SetPixelFormat(uint8_t colmod)
{
  ST7701S_WriteRegister1(ST7701_CMD_COLMOD, colmod);
}

static inline void ST7701S_SetRotation0(void)
{
  ST7701S_SetMADCTL(0x00);
}

static inline void ST7701S_SetRotation180(void)
{
  ST7701S_SetMADCTL(ST7701_MADCTL_MX | ST7701_MADCTL_MY);
}

/* =========================
   Color mode helpers
   ========================= */

/*
  BK0 register 0xCD controls color/bus mapping.
  The manufacturer init uses:
    0x08 for RGB666 mode

  For RGB565 on a 16-bit bus, we start with:
    0x00

  If colors look wrong later, 0xCD is one of the first registers to revisit.
*/
static inline void ST7701S_EnableRGB565(void)
{
  ST7701S_SelectBank0();
  ST7701S_WriteRegister1(0xCD, 0x00);
  ST7701S_SelectNormalPage();
  ST7701S_SetPixelFormat(ST7701_COLMOD_RGB565);
}

static inline void ST7701S_EnableRGB666(void)
{
  ST7701S_SelectBank0();
  ST7701S_WriteRegister1(0xCD, 0x08);
  ST7701S_SelectNormalPage();
  ST7701S_SetPixelFormat(ST7701_COLMOD_RGB666);
}

static inline void ST7701S_SetColorMode(uint8_t mode)
{
  if (mode == ST7701_COLOR_MODE_RGB666) {
    ST7701S_EnableRGB666();
  } else {
    ST7701S_EnableRGB565();
  }
}

/* =========================
   RGB interface control
   ========================= */

/*
  BK0 RGBCTRL (C3h):
    bit3 = VSP  (VSYNC polarity)
    bit2 = HSP  (HSYNC polarity)
    bit1 = DP   (DOTCLK latch edge)
    bit0 = EP   (DE/ENABLE polarity)

  DE/HV bit:
    0 = DE mode
    1 = HV mode

  For first bring-up we want:
    DE mode
    VSYNC active low
    HSYNC active low
    latch data on positive edge of PCLK
    write data when DE = 1
*/
static inline void ST7701S_SetRGBInterfaceDEMode(bool vsync_active_high = false,
                                                 bool hsync_active_high = false,
                                                 bool pclk_sample_negative_edge = false,
                                                 bool de_active_low = false)
{
  uint8_t rgbctrl0 = 0x00;  // DE/HV = 0 -> DE mode

  if (vsync_active_high)        rgbctrl0 |= (1 << 3);  // VSP
  if (hsync_active_high)        rgbctrl0 |= (1 << 2);  // HSP
  if (pclk_sample_negative_edge) rgbctrl0 |= (1 << 1); // DP
  if (de_active_low)            rgbctrl0 |= (1 << 0);  // EP

  ST7701S_SelectBank0();
  ST7701S_WriteCommand(0xC3);
  ST7701S_WriteData(rgbctrl0);
  ST7701S_WriteData(0x00);
  ST7701S_WriteData(0x00);
  ST7701S_SelectNormalPage();
}

static inline void ST7701S_SetRGBInterfaceHVMode(bool vsync_active_high = false,
                                                 bool hsync_active_high = false,
                                                 bool pclk_sample_negative_edge = false,
                                                 bool de_active_low = false,
                                                 uint8_t hbp_hv = 0x10,
                                                 uint8_t vbp_hv = 0x08)
{
  uint8_t rgbctrl0 = 0x10;  // DE/HV = 1 -> HV mode

  if (vsync_active_high)        rgbctrl0 |= (1 << 3);
  if (hsync_active_high)        rgbctrl0 |= (1 << 2);
  if (pclk_sample_negative_edge) rgbctrl0 |= (1 << 1);
  if (de_active_low)            rgbctrl0 |= (1 << 0);

  ST7701S_SelectBank0();
  ST7701S_WriteCommand(0xC3);
  ST7701S_WriteData(rgbctrl0);
  ST7701S_WriteData(hbp_hv);
  ST7701S_WriteData(vbp_hv);
  ST7701S_SelectNormalPage();
}

/* =========================
   Main initialization
   ========================= */

static inline void ST7701S_Initial(void)
{
  lcd_bus_begin();

  /* No hardware reset pin is available */
  delay(120);
  ST7701S_SoftwareReset();

  /* ---- Command2 BK3 ---- */
  ST7701S_SelectBank3();
  ST7701S_WriteRegister1(0xEF, 0x08);

  /* ---- Command2 BK0 ---- */
  ST7701S_SelectBank0();

  /* Display line setting */
  ST7701S_WriteRegister2(0xC0, 0x3B, 0x00);

  /* Porch control */
  ST7701S_WriteRegister2(0xC1, 0x0D, 0x02);

  /* Inversion / frame rate */
  ST7701S_WriteRegister2(0xC2, 0x21, 0x08);

  /*
    0xCD will be set later by ST7701S_SetColorMode().
    We intentionally skip the vendor's hard-coded RGB666 setting here.
  */

  /* Positive gamma */
  {
    static const uint8_t data[] = {
      0x00,0x11,0x18,0x0E,0x11,0x06,0x07,0x08,
      0x07,0x22,0x04,0x12,0x0F,0xAA,0x31,0x18
    };
    ST7701S_WriteBytes(0xB0, data, sizeof(data));
  }

  /* Negative gamma */
  {
    static const uint8_t data[] = {
      0x00,0x11,0x19,0x0E,0x12,0x07,0x08,0x08,
      0x08,0x22,0x04,0x11,0x11,0xA9,0x32,0x18
    };
    ST7701S_WriteBytes(0xB1, data, sizeof(data));
  }

  /* ---- Command2 BK1 ---- */
  ST7701S_SelectBank1();

  ST7701S_WriteRegister1(0xB0, 0x60);
  ST7701S_WriteRegister1(0xB1, 0x30);
  ST7701S_WriteRegister1(0xB2, 0x87);
  ST7701S_WriteRegister1(0xB3, 0x80);
  ST7701S_WriteRegister1(0xB5, 0x49);
  ST7701S_WriteRegister1(0xB7, 0x85);
  ST7701S_WriteRegister1(0xB8, 0x21);
  ST7701S_WriteRegister1(0xC1, 0x78);
  ST7701S_WriteRegister1(0xC2, 0x78);

  delay(20);

  {
    static const uint8_t data[] = {0x00,0x1B,0x02};
    ST7701S_WriteBytes(0xE0, data, sizeof(data));
  }

  {
    static const uint8_t data[] = {
      0x08,0xA0,0x00,0x00,0x07,0xA0,0x00,0x00,0x00,0x44,0x44
    };
    ST7701S_WriteBytes(0xE1, data, sizeof(data));
  }

  {
    static const uint8_t data[] = {
      0x11,0x11,0x44,0x44,0xED,0xA0,0x00,0x00,0xEC,0xA0,0x00,0x00
    };
    ST7701S_WriteBytes(0xE2, data, sizeof(data));
  }

  {
    static const uint8_t data[] = {0x00,0x00,0x11,0x11};
    ST7701S_WriteBytes(0xE3, data, sizeof(data));
  }

  {
    static const uint8_t data[] = {0x44,0x44};
    ST7701S_WriteBytes(0xE4, data, sizeof(data));
  }

  {
    static const uint8_t data[] = {
      0x0A,0xE9,0xD8,0xA0,0x0C,0xEB,0xD8,0xA0,
      0x0E,0xED,0xD8,0xA0,0x10,0xEF,0xD8,0xA0
    };
    ST7701S_WriteBytes(0xE5, data, sizeof(data));
  }

  {
    static const uint8_t data[] = {0x00,0x00,0x11,0x11};
    ST7701S_WriteBytes(0xE6, data, sizeof(data));
  }

  {
    static const uint8_t data[] = {0x44,0x44};
    ST7701S_WriteBytes(0xE7, data, sizeof(data));
  }

  {
    static const uint8_t data[] = {
      0x09,0xE8,0xD8,0xA0,0x0B,0xEA,0xD8,0xA0,
      0x0D,0xEC,0xD8,0xA0,0x0F,0xEE,0xD8,0xA0
    };
    ST7701S_WriteBytes(0xE8, data, sizeof(data));
  }

  {
    static const uint8_t data[] = {0x02,0x00,0xE4,0xE4,0x88,0x00,0x40};
    ST7701S_WriteBytes(0xEB, data, sizeof(data));
  }

  {
    static const uint8_t data[] = {0x3C,0x00};
    ST7701S_WriteBytes(0xEC, data, sizeof(data));
  }

  {
    static const uint8_t data[] = {
      0xAB,0x89,0x76,0x54,0x02,0xFF,0xFF,0xFF,
      0xFF,0xFF,0xFF,0x20,0x45,0x67,0x98,0xBA
    };
    ST7701S_WriteBytes(0xED, data, sizeof(data));
  }

  {
    static const uint8_t data[] = {0x10,0x0D,0x04,0x08,0x3F,0x1F};
    ST7701S_WriteBytes(0xEF, data, sizeof(data));
  }

  /* ---- Return to normal page ---- */
  ST7701S_SelectNormalPage();

  /* Apply board-selected color mode */
  ST7701S_SetColorMode(ST7701_DEFAULT_COLOR_MODE);

  /* ---- BK3 pre-sleep-out tweak ---- */
  ST7701S_SelectBank3();
  ST7701S_WriteRegister2(0xE8, 0x00, 0x0E);

  ST7701S_SelectNormalPage();

  /* Sleep out */
  ST7701S_SleepOut();

  /* ---- BK3 post-sleep-out tweaks ---- */
  ST7701S_SelectBank3();
  ST7701S_WriteRegister2(0xE8, 0x00, 0x0C);
  delay(10);
  ST7701S_WriteRegister2(0xE8, 0x00, 0x00);

  ST7701S_SelectNormalPage();

  /* Final format/orientation controls */
  ST7701S_SetRotation0();
  ST7701S_SetColorMode(ST7701_DEFAULT_COLOR_MODE);
  ST7701S_SetRGBInterfaceDEMode(false, false, false, false);
  ST7701S_InversionOn();
  ST7701S_DisplayOn();
}

#endif