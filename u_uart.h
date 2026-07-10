#pragma once
#include "u_ctypes.h"

#define UA_B9600 9600
#define UA_B115200 115200

#define UA_RECV_TIMEOUT_MS 30

#define udb()                 \
	uart_print("@");          \
	uart_print(__FILE__);     \
	uart_print(":");          \
	uart_print_dec(__LINE__); \
	uart_print("#[UDBridge]\n");

#define udbP(msg)                 \
	uart_print("@");          \
	uart_print(__FILE__);     \
	uart_print(":");          \
	uart_print_dec(__LINE__); \
	uart_print("#[UDBridge]: ");  \
	uart_print(msg);  \
	uart_print("\n");


static inline uint64_t read_pc()
{
	uint64_t pc = 0;
	asm volatile(
		"adr %[out], .\n"
		: [ out ] "=r"(pc));
	return pc;
}

static inline uint64_t read_sp()
{
	uint64_t sp = 0;
	asm volatile("mov %0, sp"
				 : "=r"(sp));
	return sp;
}

static inline uint64_t read_lr() {
    uint64_t x = 0;
    asm volatile("mov %0, x30" : "=r"(x));
    return x;
}

// init uart
void uart_init(unsigned long baud);

// sync send char
void uart_send(char c);

// async is new char available
int uart_available(void);

// async read (NOTE: Be sure data is ready)
char uart_read(void);

// sync read (NOTE: Be sure data is ready other wise system would loop lock in this point)
char uart_recv(int timeout);

// sync read char array to \n (NOTE: it can lock the kernel if no any input entered)
void uart_input(char *buf, unsigned int buf_size);

// sync send char array
void uart_print(const char *str);

// sync send hex
void uart_print_hex8(uint8_t hex);
void uart_print_hex16(uint16_t hex);
void uart_print_hex32(uint32_t hex);
void uart_print_hex64(uint64_t hex);

void uart_print_dec(int64_t dec);

void uart_print_float(float flt, uint32_t per);
