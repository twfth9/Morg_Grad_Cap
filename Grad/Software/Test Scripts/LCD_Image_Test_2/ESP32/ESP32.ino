#include "Debug485.h"
#include "LCD.h"

void setup()
{
  Debug485.begin(115200);

  lcd_set_verbose(true);
  //lcd_set_channel_order(LCD_CHANNEL_ORDER_GRB);
  lcd_init();
}

void loop()
{
  lcd_show_color(0xF800);
  delay(1000);
  lcd_show_color(0x07E0);
  delay(1000);
  lcd_show_color(0x001F);
  delay(1000);
  lcd_draw_vertical_bars();
  delay(1000);
}