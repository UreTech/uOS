#include "u_timer.h"

// Fix the register definitions
#define CNTP_CTL_EL0 "S3_3_C14_C2_1"
#define CNTP_TVAL_EL0 "S3_3_C14_C2_0"
#define CNTPCT_EL0 "S3_3_C14_C0_1" // Physical counter
#define CNTVCT_EL0 "S3_3_C14_C0_2" // Virtual counter
#define CNTFRQ_EL0 "S3_3_C14_C0_0" // Frequency register

// Get counter frequency
unsigned long long get_counter_freq(void)
{
	unsigned long long val;
	asm volatile("mrs %0, cntfrq_el0"
				 : "=r"(val));
	return val;
}

// Read virtual counter
unsigned long long read_cntvct(void)
{
	unsigned long long val;
	asm volatile("mrs %0, cntvct_el0"
				 : "=r"(val));
	return val;
}

time_point get_now()
{
	return read_cntvct();
}

// Convert ticks to milliseconds
unsigned long long tp_to_ms(time_point tp)
{
	static unsigned long long freq = 0;
	if (freq == 0)
	{
		freq = get_counter_freq();
	}
	return (tp * 1000ULL) / freq;
}

unsigned long long tp_to_us(time_point tp)
{
	static unsigned long long freq = 0;
	if (freq == 0)
	{
		freq = get_counter_freq();
	}
	return (tp * 1000000ULL) / freq;
}

// Delay using CPU counter (works across context switches)
void delay_ms(unsigned int ms)
{
	unsigned long long freq = get_counter_freq();
	unsigned long long ticks = (freq * ms) / 1000;
	unsigned long long start = read_cntvct();
	unsigned long long target = start + ticks;

	// Handle wrap-around properly
	while (read_cntvct() < target)
	{
		// wait...
	}
}
