/*
#include "u_canvas.h"

//debug
#include "u_uart.h"

Pixel565 P565(uint8_t red, uint8_t green, uint8_t blue)
{
	Pixel565 res = 0;
	red = ((uint16_t)red * 31) / 255;	  // 5bit
	green = ((uint16_t)green * 63) / 255; // 6bit
	blue = ((uint16_t)blue * 31) / 255;	  // 5bit

	res |= (blue << 11) & (0b11111 << 11); // 5 bits & offset 11
	res |= (green << 5) & (0b111111 << 5); // 6 bits & offset 5
	res |= (red) & (0b11111);			   // 5 bits & offset 0
	return res;
}

Pixel5658 P5658(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
	Pixel5658 res;
	res.rgb = P565(red, green, blue);
	res.alpha = alpha;
	return res;
}

Pixel5658 P5658_(Pixel565 rgb, uint32_t alpha)
{
	Pixel5658 res;
	res.rgb = rgb;
	res.alpha = alpha;
	return res;
}

Pixel5658 P565AB(Pixel5658 dst, Pixel5658 src)
{
	uint8_t dB = ((dst.rgb >> 11) & 0b11111) << 3;
	uint8_t dG = ((dst.rgb >> 5) & 0b111111) << 2;
	uint8_t dR = ((dst.rgb) & 0b11111) << 3;

	uint8_t sB = ((src.rgb >> 11) & 0b11111) << 3;
	uint8_t sG = ((src.rgb >> 5) & 0b111111) << 2;
	uint8_t sR = ((src.rgb) & 0b11111) << 3;

	uint8_t inv_alpha = 255 - src.alpha;

	uint8_t r = (sR * src.alpha + dR * inv_alpha) / 255;
	uint8_t g = (sG * src.alpha + dG * inv_alpha) / 255;
	uint8_t b = (sB * src.alpha + dB * inv_alpha) / 255;

	uint8_t al = src.alpha + (dst.alpha * inv_alpha) / 255;

	Pixel5658 result;
	result.rgb = ((b << 11) | (g << 5) | (r));
	result.alpha = al;

	return result;
}

void uc_draw_char(char ch, uint8_t x, uint8_t y, Pixel565 color, uint8_t alpha, int color_bg, u_canvas *cv)
{
	//udb();
	if (cv->dp == UC_RGB565)
	{
		// RGB565
		Pixel565 *tmpBuffer = _8x8_char_to_rgb565_buffer(ch, color);
		for (uint8_t y_ = 0; y_ < 8; y_++)
		{
			for (uint8_t x_ = 0; x_ < 8; x_++)
			{
				if ((x + x_) < cv->w && (y + y_) < cv->h)
				{
					if (color_bg || tmpBuffer[y_ * 8 + x_])
					{
						((Pixel565 *)cv->buf)[(y + y_) * cv->w + (x + x_)] = tmpBuffer[y_ * 8 + x_];
					}
				}
			}
		}
		//udb();
		vfree(tmpBuffer);
		//udb();
	}
	else
	{
		// RGBA5658
		Pixel565 *tmpBuffer = _8x8_char_to_rgb565_buffer(ch, color);
		for (uint8_t y_ = 0; y_ < 8; y_++)
		{
			for (uint8_t x_ = 0; x_ < 8; x_++)
			{
				if ((x + x_) < cv->w && (y + y_) < cv->h)
				{
					if (color_bg || tmpBuffer[y_ * 8 + x_])
					{
						((Pixel5658 *)cv->buf)[(y + y_) * cv->w + (x + x_)] = P5658_(tmpBuffer[y_ * 8 + x_], alpha);
					}
				}
			}
		}
		//	udb();
		vfree(tmpBuffer);
		//	udb();
	}

	return;
}

void uc_draw_string(const char *str, uint8_t x, uint8_t y, Pixel565 color, uint8_t alpha, u_canvas *cv, int allowNewLine)
{
	uint8_t curX = x;
	uint8_t curY = y;

	while (*str != '\0')
	{
		//	udb();
		if (curY >= 128)
		{
			break;
		}
		if (*str == '\n')
		{
			if (!allowNewLine)
			{
				break;
			}
			curY += 9; // 1 pixel space
			curX = x;
			str++;
		}
		else
		{
			if (curX + 8 >= 128)
			{
				if (!allowNewLine)
				{
					break;
				}
				curY += 9; // 1 pixel space
				curX = x;
			}
			
			uart_print_dec(curX);
			uart_print("\n");
			uart_print_dec(curY);
			uart_print("\n");
			uart_send(*str);
			uart_print("\n");
			udb();
			
			//udb();
			uc_draw_char(*str, curX, curY, color, alpha, false, cv);
			//udb();
			curX += 9; // 1 pixel space
			str++;
		}
	}
	//	udb();
	return;
}

void uc_draw_canvas(u_canvas *dst, u_canvas *src, uint32_t x, uint32_t y)
{
	if (dst->dp == UC_RGB565)
	{
		// no alpha dest
		if (src->dp == UC_RGB565)
		{
			// no alpha source
			for (uint8_t y_ = 0; y_ < src->h; y_++)
			{
				for (uint8_t x_ = 0; x_ < src->w; x_++)
				{
					if ((x + x_) < dst->w && (y + y_) < dst->h)
					{
						((Pixel565 *)dst->buf)[(y + y_) * dst->w + (x + x_)] = ((Pixel565 *)src->buf)[y_ * src->w + x_];
					}
				}
			}

				}
		else
		{
			// alpha source
			for (uint8_t y_ = 0; y_ < src->h; y_++)
			{
				for (uint8_t x_ = 0; x_ < src->w; x_++)
				{
					if ((x + x_) < dst->w && (y + y_) < dst->h)
					{
						Pixel565 *a = &((Pixel565 *)dst->buf)[(y + y_) * dst->w + (x + x_)];

						Pixel5658 *b = &((Pixel5658 *)src->buf)[y_ * src->w + x_];

						*a = P565AB(P5658_(*a, 255), *b).rgb;
					}
				}
			}

				}
	}
	else
	{
		// alpha dest
		if (src->dp == UC_RGB565)
		{
			// no alpha source
			for (uint8_t y_ = 0; y_ < src->h; y_++)
			{
				for (uint8_t x_ = 0; x_ < src->w; x_++)
				{
					if ((x + x_) < dst->w && (y + y_) < dst->h)
					{
						Pixel5658 *a = &((Pixel5658 *)dst->buf)[(y + y_) * dst->w + (x + x_)];

						Pixel565 *b = &((Pixel565 *)src->buf)[y_ * src->w + x_];

						*a = P565AB(*a, P5658_(*b, 255));
					}
				}
			}

				}
		else
		{
			// alpha source
			for (uint8_t y_ = 0; y_ < src->h; y_++)
			{
				for (uint8_t x_ = 0; x_ < src->w; x_++)
				{
					if ((x + x_) < dst->w && (y + y_) < dst->h)
					{
						Pixel5658 *a = &((Pixel5658 *)dst->buf)[(y + y_) * dst->w + (x + x_)];

						Pixel5658 *b = &((Pixel5658 *)src->buf)[y_ * src->w + x_];

						*a = P565AB(*a, *b);
					}
				}
			}

				}
	}
}

void uc_draw_canvas_active(u_canvas *dst, u_canvas *src, uint32_t x, uint32_t y)
{
	if (dst->dp == UC_RGB565)
	{
		// no alpha dest
		if (src->dp == UC_RGB565)
		{
			// no alpha source
			for (uint8_t y_ = 0; y_ < src->h; y_++)
			{
				for (uint8_t x_ = 0; x_ < src->w; x_++)
				{
					if ((x + x_) < dst->w && (y + y_) < dst->h)
					{
						((Pixel565 *)dst->buf)[(y + y_) * dst->w + (x + x_)] = ((Pixel565 *)src->active_buf)[y_ * src->w + x_]; // active/ready buf
					}
				}
			}

				}
		else
		{
			// alpha source
			for (uint8_t y_ = 0; y_ < src->h; y_++)
			{
				for (uint8_t x_ = 0; x_ < src->w; x_++)
				{
					if ((x + x_) < dst->w && (y + y_) < dst->h)
					{
						Pixel565 *a = &((Pixel565 *)dst->buf)[(y + y_) * dst->w + (x + x_)];

						Pixel5658 *b = &((Pixel5658 *)src->active_buf)[y_ * src->w + x_]; // active/ready buf

						*a = P565AB(P5658_(*a, 255), *b).rgb;
					}
				}
			}

				}
	}
	else
	{
		// alpha dest
		if (src->dp == UC_RGB565)
		{
			// no alpha source
			for (uint8_t y_ = 0; y_ < src->h; y_++)
			{
				for (uint8_t x_ = 0; x_ < src->w; x_++)
				{
					if ((x + x_) < dst->w && (y + y_) < dst->h)
					{
						Pixel5658 *a = &((Pixel5658 *)dst->buf)[(y + y_) * dst->w + (x + x_)];

						Pixel565 *b = &((Pixel565 *)src->active_buf)[y_ * src->w + x_]; // active/ready buf

						*a = P565AB(*a, P5658_(*b, 255));
					}
				}
			}

				}
		else
		{
			// alpha source
			for (uint8_t y_ = 0; y_ < src->h; y_++)
			{
				for (uint8_t x_ = 0; x_ < src->w; x_++)
				{
					if ((x + x_) < dst->w && (y + y_) < dst->h)
					{
						Pixel5658 *a = &((Pixel5658 *)dst->buf)[(y + y_) * dst->w + (x + x_)];

						Pixel5658 *b = &((Pixel5658 *)src->active_buf)[y_ * src->w + x_]; // active/ready buf

						*a = P565AB(*a, *b);
					}
				}
			}

				}
	}
}

void uc_clear_canvas(u_canvas *cv, Pixel5658 color)
{
	if (cv->dp == UC_RGB565)
	{
		// RGB565
		memfill(cv->buf, &color.rgb, sizeof(Pixel565), cv->w * cv->h);
	}
	else
	{
		// RGBA5658
		memfill(cv->buf, &color, sizeof(Pixel5658), cv->w * cv->h);
	}
}

void uc_swap_buffers(u_canvas *cv)
{
	if (cv->bst == UC_DOUBLE_BUFFER)
	{
		void *tmp = cv->active_buf;
		cv->active_buf = cv->buf;
		cv->buf = tmp;
	}
}

u_canvas *uc_resize_canvas(uint32_t w, uint32_t h, u_canvas *cv)
{
	size_t tsize = 0;
	switch (cv->dp)
	{
	case UC_RGB565:
		tsize = 2; // 2bytes
		break;
	case UC_RGBA5658:
		tsize = 3; // 3bytes
		break;
	case UC_FLOAT32:
		tsize = 4; // 4bytes
		break;
	default:
		return cv;
		break;
	};
	void *new = vrealloc(cv->buf, tsize * w * h);

	if (new != NULL)
	{
		cv->buf = new;
		cv->w = w;
		cv->h = h;
	}

	if (cv->bst == UC_DOUBLE_BUFFER)
	{
		void *new_a = vrealloc(cv->active_buf, tsize * w * h);
		if (new_a != NULL)
		{
			cv->active_buf = new_a;
		}
	}
	else
	{
		if (new != NULL)
		{
			cv->active_buf = new;
		}
	}

	return cv;
}

u_canvas *uc_new_canvas(uint32_t w, uint32_t h, uc_buffer_depth dp, uc_buffer_struct_type bsc)
{
	u_canvas *new = (u_canvas *)malloc(sizeof(u_canvas));

	size_t tsize = 0;
	switch (dp)
	{
	case UC_RGB565:
		tsize = 2; // 2bytes
		break;
	case UC_RGBA5658:
		tsize = 3; // 3bytes
		break;
	case UC_FLOAT32:
		tsize = 4; // 4bytes
		break;
	default:
		new->w = 0;
		new->h = 0;
		return new;
	};

	new->w = w;
	new->h = h;
	new->dp = dp;
	new->bst = bsc;
	if (new->bst == UC_DOUBLE_BUFFER)
	{
		new->buf = vmalloc(tsize * w * h);		  // must be unique to program
		new->active_buf = vmalloc(tsize * w * h); // must be unique to program
	}
	else
	{
		new->buf = vmalloc(tsize * w * h); // must be unique to program
		new->active_buf = new->buf;
	}
	return new;
}

void uc_destroy_canvas(u_canvas *cv)
{
	vfree(cv->buf);
	if (cv->bst == UC_DOUBLE_BUFFER)
	{
		vfree(cv->active_buf);
	}
	free(cv);
}

*/