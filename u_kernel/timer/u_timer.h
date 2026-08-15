#define SYSTEM_TIMER_BASE 0xFE003000
#define SYS_TIMER_CLO ((volatile unsigned int *)(SYSTEM_TIMER_BASE + 0x04)) // Lower 32-bit counter

#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/framework/interrupt/u_interrupt.h>

typedef unsigned long long time_point;

void delay_ms(unsigned int ms);

time_point get_now();

unsigned long long tp_to_ms(time_point tp);

unsigned long long tp_to_us(time_point tp);

// debug
unsigned long long get_counter_freq(void);
