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

#define SPI0_BASE       (GPIO_BASE + 0x204000)
#define SPI0_CS         ((volatile uint32_t*)(SPI0_BASE + 0x00))
#define SPI0_FIFO       ((volatile uint32_t*)(SPI0_BASE + 0x04))
#define SPI0_CLK        ((volatile uint32_t*)(SPI0_BASE + 0x08))
#define SPI0_DLEN       ((volatile uint32_t*)(SPI0_BASE + 0x0C))
#define SPI0_LTOH       ((volatile uint32_t*)(SPI0_BASE + 0x10))
#define SPI0_DC         ((volatile uint32_t*)(SPI0_BASE + 0x14))

void mmio_write(long reg, unsigned int val);
unsigned int mmio_read(long reg);

unsigned int gpio_call(unsigned int pin_number, unsigned int value, unsigned int base, unsigned int field_size, unsigned int field_max);

unsigned int gpio_set     (unsigned int pin_number, unsigned int value);
unsigned int gpio_clear   (unsigned int pin_number, unsigned int value);
unsigned int gpio_pull    (unsigned int pin_number, unsigned int value);
unsigned int gpio_function(unsigned int pin_number, unsigned int value);

void gpio_useAsAlt5(unsigned int pin_number);

void pinMode(int pin, int mode);

void digitalWrite(int pin, int value);

//spio
void spi_init();

void spi_chip_select(unsigned char chip_select);

void spi_send_recv(unsigned char *sbuffer, unsigned char *rbuffer, unsigned int size);

void spi_send(unsigned char *data, unsigned int size);

void spi_recv(unsigned char *data, unsigned int size);
