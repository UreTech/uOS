#include <u_kernel/drivers/gpio/u_gpio.h>
#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/timer/u_timer.h>
#include <u_kernel/drivers/uart/u_uart.h>

void pinMode(int pin, int mode)
{
	volatile unsigned int *fsel_reg = GPFSEL0 + (pin / 10);
	int shift = (pin % 10) * 3;

	unsigned int value = *fsel_reg;
	value &= ~(7 << shift);
	if (mode == OUTPUT)
	{
		value |= (1 << shift);
	}
	*fsel_reg = value;
}

void digitalWrite(int pin, int value)
{
	if (value == HIGH)
	{
		if (pin < 32)
		{
			*(u32 *)GPSET0 = (1 << pin);
		}
		else
		{
			*GPSET1 = (1 << (pin - 32));
		}
	}
	else
	{
		if (pin < 32)
		{
			*(u32 *)GPCLR0 = (1 << pin);
		}
		else
		{
			*GPCLR1 = (1 << (pin - 32));
		}
	}
}

void gpio_set_function(unsigned pin, gpio_func_t func)
{
	if (pin > 53)
		return;

	unsigned reg_index = pin / 10;

	unsigned bit_shift = (pin % 10) * 3;

	volatile uint32_t *gpfsel = GPFSEL0;
	switch (reg_index)
	{
	case 0:
		gpfsel = GPFSEL0;
		break;
	case 1:
		gpfsel = GPFSEL1;
		break;
	case 2:
		gpfsel = GPFSEL2;
		break;
	case 3:
		gpfsel = GPFSEL3;
		break;
	case 4:
		gpfsel = GPFSEL4;
		break;
	case 5:
		gpfsel = GPFSEL5;
		break;
	default:
		return;
	}

	uint32_t mask = 7u << bit_shift;

	uint32_t val = ((uint32_t)func << bit_shift) & mask;

	uint32_t cur = *gpfsel;
	cur &= ~mask;
	cur |= val;
	*gpfsel = cur;
}

#define LED_GPIO_BIT_IN_FSEL 2   // field index within GPFSEL4 (each field is 3 bits)
#define LED_GPIO_BIT_IN_SETCLR 10 // bit index within GPSET1/GPCLR1

void led_init(void)
{
    unsigned int r = *GPFSEL4;
    r &= ~(7u << (LED_GPIO_BIT_IN_FSEL * 3)); // clear field
    r |= (1u << (LED_GPIO_BIT_IN_FSEL * 3));  // set as output (001)
    *GPFSEL4 = r;
}

void led_on(void)
{
    *GPSET1 = (1u << LED_GPIO_BIT_IN_SETCLR);
}

void led_off(void)
{
    *GPCLR1 = (1u << LED_GPIO_BIT_IN_SETCLR);
}

void led_blink_forever(void)
{
    while (1)
    {
        led_on();
        for (volatile unsigned int i = 0; i < 3000000; i++) {}
        led_off();
        for (volatile unsigned int i = 0; i < 3000000; i++) {}
    }
}