#define LCD_8BIT_INTERFACE_128x128
#ifdef LCD_8BIT_INTERFACE_128x128
#include"u_gpio.h" // basic gpio
#include"u_timer.h" // basic timer
#include"u_kernelBaseFont.h"
#include"u_ctypes.h"

#define LCD_0 4 //4
#define LCD_1 17 //17
#define LCD_2 27 //27
#define LCD_3 22 //22
#define LCD_4 10 //10
#define LCD_5 9 //9
#define LCD_6 11 //11
#define LCD_7 5 //5
#define LCD_WR 12 //12 // write
#define LCD_RS 16 //16 // register select 0 command 1 data
#define LCD_CS 20 //20 // chip select
#define LCD_BL 21 //21 // back light
#define LCD_RST 13 //13 // reset pin

#define COMMAND_REGISTER 0
#define DATA_REGISTER 1

typedef int16_t Pixel565;

void lcd_send_data(u8 data, bool register_);

extern u8 backLight;

void lcd_set_back_light(u8 in);

void lcd_sleep(bool off);

void lcd_hardware_reset();

void lcd_init();

// ce enable
void lcd_gpio_enable();

// ce disable
void lcd_gpio_disable();

void lcd_set_address_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);

void lcd_draw_pixel(uint8_t x, uint8_t y, Pixel565 color);

void lcd_draw_bitmap(uint8_t x, uint8_t y, Pixel565* buffer, uint8_t w, uint8_t h);

void lcd_draw_char(char ch, uint8_t x, uint8_t y, Pixel565 color, Pixel565* bufferToDraw);

void lcd_draw_string(const char* str, uint8_t x, uint8_t y, Pixel565 color, Pixel565* bufferToDraw);

void lcd_clear_screen(uint16_t color);

#endif