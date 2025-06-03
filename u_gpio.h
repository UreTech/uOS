#include"u_ctypes.h"

// general gpio
#define GPIO_BASE 0xFE200000UL  // Raspberry Pi 4 için
#define GPFSEL1   ((volatile unsigned int*)(GPIO_BASE + 0x04))
#define GPFSEL2   ((volatile unsigned int*)(GPIO_BASE + 0x08))
#define GPFSEL3   ((volatile unsigned int*)(GPIO_BASE + 0x0C))
#define GPFSEL4   ((volatile unsigned int*)(GPIO_BASE + 0x10))
#define GPFSEL5   ((volatile unsigned int*)(GPIO_BASE + 0x14))
#define GPSET1    ((volatile unsigned int*)(GPIO_BASE + 0x20))
#define GPCLR1    ((volatile unsigned int*)(GPIO_BASE + 0x2C))

#define GPPUD           ((volatile uint32_t*)(GPIO_BASE + 0x200094))
#define GPPUDCLK0       ((volatile uint32_t*)(GPIO_BASE + 0x200098))

#define INPUT  0
#define OUTPUT 1

#define LOW    0
#define HIGH   1

void pinMode(int pin, int mode);

void digitalWrite(int pin, int value);

