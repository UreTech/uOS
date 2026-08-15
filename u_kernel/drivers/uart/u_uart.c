#include <u_kernel/drivers/gpio/u_gpio.h>
#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/timer/u_timer.h>
#include <u_kernel/drivers/uart/u_uart.h>
#include <u_kernel/util/u_cstr_util.h>
#include <u_kernel/util/lock/u_mutex.h>

#define UART0_BASE 0xFE201000

#define UART0_DR (*(volatile unsigned int *)(UART0_BASE + 0x00))   // Data Register
#define UART0_FR (*(volatile unsigned int *)(UART0_BASE + 0x18))   // Flag Register
#define UART0_IBRD (*(volatile unsigned int *)(UART0_BASE + 0x24)) // Integer Baud rate
#define UART0_FBRD (*(volatile unsigned int *)(UART0_BASE + 0x28)) // Fractional Baud rate
#define UART0_LCRH (*(volatile unsigned int *)(UART0_BASE + 0x2C)) // Line Control
#define UART0_CR (*(volatile unsigned int *)(UART0_BASE + 0x30))   // Control
#define UART0_ICR (*(volatile unsigned int *)(UART0_BASE + 0x44))  // Interrupt Clear

void gpio_uart_init(void)
{
	unsigned int r;

	r = *GPFSEL1;
	r &= ~((7 << 12) | (7 << 15));
	r |= (4 << 12) | (4 << 15);
	*GPFSEL1 = r;

	*GPPUD = 0;
	delay_ms(15);
	*GPPUDCLK0 = (1 << 14) | (1 << 15);
	delay_ms(15);
	*GPPUDCLK0 = 0;
}

u_mutex uart_mutex;

void uart_init(unsigned long baud)
{
	// uart gpio ALT0 selection
	gpio_uart_init();

	UART0_CR = 0x00000000;
	UART0_ICR = 0x7FF;

	// baud selection
	if (baud == UA_B115200)
	{
		UART0_IBRD = 26;
		UART0_FBRD = 3;
	}
	else if (baud == UA_B9600)
	{
		UART0_IBRD = 312;
		UART0_FBRD = 32;
	}
	else
	{
		// default 115200
		UART0_IBRD = 26;
		UART0_FBRD = 3;
	}

	UART0_LCRH = (1 << 4) | (1 << 5) | (1 << 6);
	UART0_CR = (1 << 0) | (1 << 8) | (1 << 9);

	// init mutex
	u_mutex_init(&uart_mutex);
}

void uart_send(char c)
{
	time_point start = get_now();
	while (UART0_FR & (1 << 5))
	{
		if (tp_to_ms(get_now() - start) >= 1000)
		{
			return; // timeout
		}
	}
	UART0_DR = c;
}

int uart_available(void)
{
	return !(UART0_FR & (1 << 4));
}

char uart_read(void)
{
	return (char)(UART0_DR & 0xFF);
}

char uart_recv(int timeout)
{
	time_point start = get_now();
	while (!uart_available())
	{
		if (timeout != 0)
		{
			if (tp_to_ms(get_now() - start) >= timeout)
			{
				return 0xFF; // timeout
			}
		}
	}
	return uart_read();
}

void uart_input(char *buf, unsigned int buf_size)
{
	unsigned int cur = 0;

	// wait for input...
	while (!uart_available())
	{
	}

	while (true)
	{
		if (buf_size < cur + 1)
		{
			break;
		}

		char input = uart_recv(UA_RECV_TIMEOUT_MS);

		if (input == '\n' || input == '\r')
		{
			break;
		}
		else if (input == 0xFF)
		{
			break; // timeout
		}
		else
		{
			buf[cur] = input;
			cur++;
		}
	}
	buf[cur] = '\0';
}

void uart_print(const char *str)
{
	while (*str)
	{
		uart_send(*str++);
	}
}

void uart_print_hex8(uint8_t hex)
{
	char tmpstr[30];
	uart_print(ulltohexa(hex, tmpstr));
}

void uart_print_hex16(uint16_t hex)
{
	char tmpstr[30];
	uart_print(ulltohexa(hex, tmpstr));
}

void uart_print_hex32(uint32_t hex)
{
	char tmpstr[30];
	uart_print(ulltohexa(hex, tmpstr));
}

void uart_print_hex64(uint64_t hex)
{
	char tmpstr[30];
	uart_print(ulltohexa(hex, tmpstr));
}

void uart_print_dec(int64_t dec)
{
	char tmpstr[30];
	uart_print(lltoa(dec, tmpstr));
}

void uart_print_float(float flt, uint32_t per)
{
	char tmpstr[50];
	uart_print(ftoa(flt, per, tmpstr));
}