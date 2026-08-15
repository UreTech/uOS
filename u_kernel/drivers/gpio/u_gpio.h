#ifndef U_GPIO_H
#define U_GPIO_H
#include <u_kernel/util/u_ctypes.h>

// general gpio
#define GPIO_BASE 0xFE200000UL

#define GPFSEL1 ((volatile unsigned int *)(GPIO_BASE + 0x04))
#define GPFSEL2 ((volatile unsigned int *)(GPIO_BASE + 0x08))
#define GPFSEL3 ((volatile unsigned int *)(GPIO_BASE + 0x0C))
#define GPFSEL4 ((volatile unsigned int *)(GPIO_BASE + 0x10))
#define GPFSEL5 ((volatile unsigned int *)(GPIO_BASE + 0x14))

#define GPSET0 ((volatile unsigned int *)(GPIO_BASE + 0x1C))
#define GPCLR0 ((volatile unsigned int *)(GPIO_BASE + 0x28))

#define GPSET1 ((volatile unsigned int *)(GPIO_BASE + 0x20))
#define GPCLR1 ((volatile unsigned int *)(GPIO_BASE + 0x2C))

#define GPFSEL0 ((volatile unsigned int *)(GPIO_BASE + 0x00))
#define GPLEV0 ((volatile unsigned int *)(GPIO_BASE + 0x34))

#define GPPUD ((volatile uint32_t *)(GPIO_BASE + 0x200094))
#define GPPUDCLK0 ((volatile uint32_t *)(GPIO_BASE + 0x200098))

#define INPUT 0
#define OUTPUT 1

#define LOW 0
#define HIGH 1

typedef enum
{
	GPIO_FUNC_INPUT = 0,
	GPIO_FUNC_OUTPUT = 1,
	GPIO_FUNC_ALT0 = 4,
	GPIO_FUNC_ALT1 = 5,
	GPIO_FUNC_ALT2 = 6,
	GPIO_FUNC_ALT3 = 7,
	GPIO_FUNC_ALT4 = 3,
	GPIO_FUNC_ALT5 = 2
} gpio_func_t;

void gpio_set_function(unsigned pin, gpio_func_t func);

void pinMode(int pin, int mode);

void digitalWrite(int pin, int value);

// builtin ACT led
void led_init(void);
void led_on(void);
void led_off(void);
void led_blink_forever(void);

#endif