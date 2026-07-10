#define U_KERNEL_BASE_FONT_H


#ifndef U_KERNEL_BASE_FONT_H
#define U_KERNEL_BASE_FONT_H

#include"u_ctypes.h"

typedef int16_t Pixel565;

extern unsigned char console_font_8x8[];

Pixel565 makeRGB565(u8 R, u8 G, u8 B);

Pixel565* _8x8_char_to_rgb565_buffer(char ch, Pixel565 color);
#endif