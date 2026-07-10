#define U_CANVAS_H
#ifndef U_CANVAS_H
#define U_CANVAS_H

#include "u_kernelBaseFont.h"
#include "u_ctypes.h"

#include <memory/u_memory.h>

typedef int16_t Pixel565;

typedef enum
{
	UC_RGB565 = 0xA0,
	UC_RGBA5658 = 0xA1,
	UC_FLOAT32 = 0xF1
} uc_buffer_depth;

typedef enum
{
	UC_SINGLE_BUFFER = 0xEE,
	UC_DOUBLE_BUFFER = 0xDD,
} uc_buffer_struct_type;

typedef struct
{
	Pixel565 rgb;
	uint8_t alpha;
} Pixel5658;

typedef struct
{
	void *buf;
	void *active_buf;
	uc_buffer_struct_type bst;
	uint32_t w, h;
	uc_buffer_depth dp;
} u_canvas;

// make Pixel565 (255/255/255) -> (31/63/31) div[8/4/8]
Pixel565 P565(uint8_t red, uint8_t green, uint8_t blue);

// P565 -> P5658
Pixel5658 P5658_(Pixel565 rgb, uint32_t alpha);

// alpha blend
Pixel5658 P565AB(Pixel5658 dst, Pixel5658 src);

Pixel5658 P5658(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);

// draw char
void uc_draw_char(char ch, uint8_t x, uint8_t y, Pixel565 color, uint8_t alpha, int color_bg, u_canvas *cv);

// draw string
void uc_draw_string(const char *str, uint8_t x, uint8_t y, Pixel565 color, uint8_t alpha, u_canvas *cv, int allowNewLine);

// draw other buffer on top
void uc_draw_canvas(u_canvas *dst, u_canvas *src, uint32_t x, uint32_t y);

// draw other buffer on top (src active to dst not ready buffer)
void uc_draw_canvas_active(u_canvas *dst, u_canvas *src, uint32_t x, uint32_t y);

// clear canvas
void uc_clear_canvas(u_canvas *cv, Pixel5658 color);

// swap buffers if canvas is double buffered
void uc_swap_buffers(u_canvas *cv);

// resize canvas
u_canvas *uc_resize_canvas(uint32_t w, uint32_t h, u_canvas *cv);

// create new canvas
u_canvas *uc_new_canvas(uint32_t w, uint32_t h, uc_buffer_depth dp, uc_buffer_struct_type bsc);

// destroy canvas
void uc_destroy_canvas(u_canvas *cv);

#endif