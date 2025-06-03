#include"u_gpio.h"
#include"u_ctypes.h"
#include"u_timer.h"
#include"u_uart.h"

#define GPFSEL0    ((volatile unsigned int*)(GPIO_BASE + 0x00))  // GPIO Function Select
#define GPSET0     ((volatile unsigned int*)(GPIO_BASE + 0x1C))  // GPIO Pin Output Set
#define GPCLR0     ((volatile unsigned int*)(GPIO_BASE + 0x28))  // GPIO Pin Output Clear
#define GPLEV0     ((volatile unsigned int*)(GPIO_BASE + 0x34))  // GPIO Pin Level


void pinMode(int pin, int mode){
    volatile unsigned int* fsel_reg = GPFSEL0 + (pin / 10);
    int shift = (pin % 10) * 3;

    unsigned int value = *fsel_reg;
    value &= ~(7 << shift);  // Bitleri temizle
    if (mode == OUTPUT) {
        value |= (1 << shift);  // 001: Output mode
    }
    *fsel_reg = value;
}

void digitalWrite(int pin, int value) {
    if (value == HIGH) {
        if (pin < 32) {
            *(u32*)GPSET0 = (1 << pin);
        } else {
            *GPSET1 = (1 << (pin - 32));
        }
    } else {
        if (pin < 32) {
            *(u32*)GPCLR0 = (1 << pin);
        } else {
            *GPCLR1 = (1 << (pin - 32));
        }
    }
}