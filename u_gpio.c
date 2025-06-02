#include"u_gpio.h"
#include"u_ctypes.h"
#include"u_timer.h"
#include"u_uart.h"

enum {
    GPFSEL0         = GPIO_BASE + 0x200000,
    GPSET0          = GPIO_BASE + 0x20001C,
    GPCLR0          = GPIO_BASE + 0x200028,
    GPPUPPDN0       = GPIO_BASE + 0x2000E4
};

enum {
    GPIO_MAX_PIN       = 53,
    GPIO_FUNCTION_OUT  = 1,
    GPIO_FUNCTION_ALT5 = 2,
    GPIO_FUNCTION_ALT3 = 7,
    GPIO_FUNCTION_ALT0 = 4,
};

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

// util

unsigned int gpio_call(unsigned int pin_number, unsigned int value, unsigned int base, unsigned int field_size, unsigned int field_max) {
    unsigned int field_mask = (1 << field_size) - 1;
  
    if (pin_number > field_max) return 0;
    if (value > field_mask) return 0; 

    unsigned int num_fields = 32 / field_size;
    unsigned int reg = base + ((pin_number / num_fields) * 4);
    unsigned int shift = (pin_number % num_fields) * field_size;

    unsigned int curval = *(volatile unsigned int *)reg;
    curval &= ~(field_mask << shift);
    curval |= value << shift;
    *(volatile unsigned int *)reg = curval;

    return 1;
}

unsigned int gpio_set     (unsigned int pin_number, unsigned int value) { return gpio_call(pin_number, value, GPSET0, 1, GPIO_MAX_PIN); }
unsigned int gpio_clear   (unsigned int pin_number, unsigned int value) { return gpio_call(pin_number, value, GPCLR0, 1, GPIO_MAX_PIN); }

void mmio_write(long reg, unsigned int val) { *(volatile unsigned int *)reg = val; }
unsigned int mmio_read(long reg) { return *(volatile unsigned int *)reg; }

unsigned int gpio_function(unsigned int pin_number, unsigned int value) { 
    return gpio_call(pin_number, value, GPFSEL0, 3, GPIO_MAX_PIN); 
}

unsigned int gpio_pull(unsigned int pin_number, unsigned int value) 
{ return gpio_call(pin_number, value, GPPUPPDN0, 2, GPIO_MAX_PIN); 
}

void gpio_useAsAlt0(unsigned int pin_number) {
    gpio_pull(pin_number, 0);
    gpio_function(pin_number, GPIO_FUNCTION_ALT0);
}

void gpio_useAsAlt5(unsigned int pin_number) {
    gpio_pull(pin_number, 0);
    gpio_function(pin_number, GPIO_FUNCTION_ALT5);
}

void gpio_initOutputPinWithPullNone(unsigned int pin_number) {
    gpio_pull(pin_number, 0);
    gpio_function(pin_number, GPIO_FUNCTION_OUT);
}


// SPI

struct Spi0Regs {
    volatile unsigned int cs;
    volatile unsigned int fifo;
    volatile unsigned int clock;
    volatile unsigned int data_length;
    volatile unsigned int ltoh;
    volatile unsigned int dc;
};

#define REGS_SPI0 ((struct Spi0Regs *)(GPIO_BASE + 0x00204000))

// CS Register
#define CS_LEN_LONG	(1 << 25)
#define CS_DMA_LEN	(1 << 24)
#define CS_CSPOL2	(1 << 23)
#define CS_CSPOL1	(1 << 22)
#define CS_CSPOL0	(1 << 21)
#define CS_RXF		(1 << 20)
#define CS_RXR		(1 << 19)
#define CS_TXD		(1 << 18)
#define CS_RXD		(1 << 17)
#define CS_DONE		(1 << 16)
#define CS_LEN		(1 << 13)
#define CS_REN		(1 << 12)
#define CS_ADCS		(1 << 11)
#define CS_INTR		(1 << 10)
#define CS_INTD		(1 << 9)
#define CS_DMAEN	(1 << 8)
#define CS_TA		(1 << 7)
#define CS_CSPOL	(1 << 6)
#define CS_CLEAR_RX	(1 << 5)
#define CS_CLEAR_TX	(1 << 4)
#define CS_CPOL__SHIFT	3
#define CS_CPHA__SHIFT	2
#define CS_CS		(1 << 0)
#define CS_CS__SHIFT	0

void spi_init() {
    gpio_useAsAlt0(7);                  //CS1
    gpio_initOutputPinWithPullNone(8);  //CS0
    gpio_useAsAlt0(9);                  //MISO 
    gpio_useAsAlt0(10);                 //MOSI
    gpio_useAsAlt0(11);                 //SCLK
}

void spi_chip_select(unsigned char chip_select) {
    //gpio_setPinOutputBool(8, chip_select);
    pinMode(chip_select, OUTPUT);
}

void spi_send_recv(unsigned char *sbuffer, unsigned char *rbuffer, unsigned int size) {
    REGS_SPI0->data_length = size;
    REGS_SPI0->cs = REGS_SPI0->cs | CS_CLEAR_RX | CS_CLEAR_TX | CS_TA;
    
    unsigned int read_count = 0;
    unsigned int write_count = 0;

    while(read_count < size || write_count < size) {
        while(write_count < size && REGS_SPI0->cs & CS_TXD) {
            if (sbuffer) {
                REGS_SPI0->fifo = *sbuffer++;
            } else {
                REGS_SPI0->fifo = 0;
            }

            write_count++;
        }

        while(read_count < size && REGS_SPI0->cs & CS_RXD) {
            unsigned int data = REGS_SPI0->fifo;

            if (rbuffer) {
                *rbuffer++ = data;
            }

            read_count++;
        }
    }

    while(!(REGS_SPI0->cs & CS_DONE)) {
        while(REGS_SPI0->cs & CS_RXD) {
            unsigned int r = REGS_SPI0->fifo;
	    //debughex(r);
        }
    }

    REGS_SPI0->cs = (REGS_SPI0->cs & ~CS_TA);
}

void spi_send(unsigned char *data, unsigned int size) {
    spi_send_recv(data, 0, size);
}

void spi_recv(unsigned char *data, unsigned int size) {
    spi_send_recv(0, data, size);
}