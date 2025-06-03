#include"u_display.h" // basic display
#include"u_gpio.h" // basic gpio
#include"u_timer.h" // basic timer
#include"u_kernelBaseFont.h"
#include"u_ctypes.h"
#include"u_heap.h"

typedef int16_t Pixel565;

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

void lcd_send_data(u8 data, bool register_){
	// writing to bus
	digitalWrite(LCD_0, (data >> 0) & 1);
	digitalWrite(LCD_1, (data >> 1) & 1);
	digitalWrite(LCD_2, (data >> 2) & 1);
	digitalWrite(LCD_3, (data >> 3) & 1);
	digitalWrite(LCD_4, (data >> 4) & 1);
	digitalWrite(LCD_5, (data >> 5) & 1);
	digitalWrite(LCD_6, (data >> 6) & 1);
	digitalWrite(LCD_7, (data >> 7) & 1);
	
	digitalWrite(LCD_RS, register_);
	// send it
	digitalWrite(LCD_WR, 0);
	digitalWrite(LCD_WR, 1);
}

u8 backLight = 64;

void lcd_set_back_light(u8 in){
	backLight = in;
	digitalWrite(LCD_BL, 0xff - backLight); // back light pwm (not works right now)
}

void lcd_sleep(bool off){
	lcd_gpio_enable();
	if(off){
		lcd_send_data(0x28, COMMAND_REGISTER);
		lcd_set_back_light(0);
	}else{
		lcd_send_data(0x29, COMMAND_REGISTER);
		lcd_set_back_light(backLight);
	}
	lcd_gpio_disable();
}

void lcd_hardware_reset(){
	digitalWrite(LCD_RST, 0);
	delay_ms(10);
	digitalWrite(LCD_RST, 1);
	delay_ms(15);
}

void lcd_init() {
	pinMode(LCD_0, OUTPUT);
	pinMode(LCD_1, OUTPUT);
	pinMode(LCD_2, OUTPUT);
	pinMode(LCD_3, OUTPUT);
	pinMode(LCD_4, OUTPUT);
	pinMode(LCD_5, OUTPUT);
	pinMode(LCD_6, OUTPUT);
	pinMode(LCD_7, OUTPUT);
	pinMode(LCD_WR, OUTPUT);
	pinMode(LCD_RS, OUTPUT);
	pinMode(LCD_CS, OUTPUT);
	pinMode(LCD_BL, OUTPUT);
	pinMode(LCD_RST, OUTPUT);
	//lcd reset
	lcd_hardware_reset();
	
	lcd_gpio_enable(); // enable display
	digitalWrite(LCD_RS, 0); // default
	digitalWrite(LCD_WR, 1); // default disabled(true)

    digitalWrite(LCD_BL, 0); // back light pwm

	lcd_hardware_reset();
	
	lcd_send_data(0x01, COMMAND_REGISTER); // Software Reset
    delay_ms(150);

    lcd_send_data(0x11, COMMAND_REGISTER); // Sleep Out
    delay_ms(120);

    lcd_send_data(0x3A, COMMAND_REGISTER); // Interface Pixel Format
    lcd_send_data(0x05, DATA_REGISTER);    // 16-bit/pixel (RGB565)

    lcd_send_data(0x36, COMMAND_REGISTER); // Memory Data Access Control
    lcd_send_data(0x00, DATA_REGISTER);    // Row/col order (değiştirilebilir)

    lcd_send_data(0x29, COMMAND_REGISTER); // Display ON
    delay_ms(100);
	lcd_gpio_disable();	
}

	
void lcd_gpio_enable(){
	digitalWrite(LCD_RS, 0); // default
	digitalWrite(LCD_WR, 1); // default disabled(true)
	lcd_set_back_light(backLight);
	digitalWrite(LCD_CS, 0); // enable display
}

void lcd_gpio_disable(){
	digitalWrite(LCD_CS, 1); // disable display
}

void lcd_set_address_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    lcd_send_data(0x2A, COMMAND_REGISTER); // Column Address Set
    lcd_send_data(0x00, DATA_REGISTER);
    lcd_send_data(x0, DATA_REGISTER);
    lcd_send_data(0x00, DATA_REGISTER);
    lcd_send_data(x1, DATA_REGISTER);

    lcd_send_data(0x2B, COMMAND_REGISTER); // Row Address Set
    lcd_send_data(0x00, DATA_REGISTER);
    lcd_send_data(y0, DATA_REGISTER);
    lcd_send_data(0x00, DATA_REGISTER);
    lcd_send_data(y1, DATA_REGISTER);

    lcd_send_data(0x2C, COMMAND_REGISTER); // Memory Write
}

void lcd_draw_pixel(uint8_t x, uint8_t y, Pixel565 color) {
	lcd_gpio_enable();
    lcd_set_address_window(x, y, x, y);
    lcd_send_data(color & 0xFF, DATA_REGISTER);
    lcd_send_data(color >> 8, DATA_REGISTER);
	lcd_gpio_disable();
}

void lcd_draw_bitmap(uint8_t x, uint8_t y, Pixel565* buffer, uint8_t w, uint8_t h){
	lcd_gpio_enable();
    if (x + w > 127) {
        w = 128 - x;
    }
    if (y + h > 127) {
        h = 128 - y;
    }
    lcd_set_address_window(x, y, x + w - 1, y + h - 1);
    for (int y_ = 0; y_ < h; y_++) {
        for (int x_ = 0; x_ < w; x_++) {
            uint16_t color = buffer[(y_ * w) + x_];
            lcd_send_data(color >> 8, DATA_REGISTER);
            lcd_send_data(color & 0xFF, DATA_REGISTER);
        }
    }
	lcd_gpio_disable();
}

void lcd_draw_char(char ch, uint8_t x, uint8_t y, Pixel565 color, Pixel565* bufferToDraw) {
	Pixel565* tmpBuffer = _8x8_char_to_rgb565_buffer(ch, color);
	for (uint8_t y_ = 0; y_ < 8; y_++) {
		for (uint8_t x_ = 0; x_ < 8; x_++) {
			bufferToDraw[(y + y_) * 128 + (x + x_)] = tmpBuffer[y_ * 8 + x_];
		}
	}
	u_free(tmpBuffer);
}

void lcd_draw_string(const char* str, uint8_t x, uint8_t y, Pixel565 color, Pixel565* bufferToDraw){
	uint8_t curX = x;
	uint8_t curY = y;

	while(*str != '\0'){
		if(*str == '\n'){
			curY += 9; // 1 pixel space
			curX = x;
			str++;
		}else{
			lcd_draw_char(*str, curX, curY, color, bufferToDraw);
			curX += 9; // 1 pixel space
			str++;
		}
		if(curX >= 128){
			curY += 9; // 1 pixel space
			curX = x;
		}
	}
}

void lcd_clear_screen(uint16_t color) {
	lcd_gpio_enable();
    lcd_set_address_window(0, 0, 127, 127);

    lcd_send_data(0x2C, COMMAND_REGISTER);

    for (int i = 0; i < 128 * 128; ++i) {
        lcd_send_data(color >> 8, DATA_REGISTER);
        lcd_send_data(color & 0xFF, DATA_REGISTER);
    }
	lcd_gpio_disable();
}