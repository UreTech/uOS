#include "u_ctypes.h"
#include "u_gpio.h"
#include "u_timer.h"
#include "u_uart.h"

#define SPI0_BASE 0xFE204000UL
#define SPI0_CS (*(volatile uint32_t *)(SPI0_BASE + 0x00))
#define SPI0_FIFO (*(volatile uint32_t *)(SPI0_BASE + 0x04))
#define SPI0_CLK (*(volatile uint32_t *)(SPI0_BASE + 0x08))
#define SPI0_DLEN (*(volatile uint32_t *)(SPI0_BASE + 0x0C))

#define SPI_CS_LEN_LONG (1 << 25)
#define SPI_CS_DMA_LEN (1 << 24)
#define SPI_CS_CSPOL2 (1 << 23)
#define SPI_CS_CSPOL1 (1 << 22)
#define SPI_CS_CSPOL0 (1 << 21)
#define SPI_CS_RXF (1 << 20)
#define SPI_CS_RXR (1 << 19)
#define SPI_CS_TXD (1 << 18)
#define SPI_CS_RXD (1 << 17)
#define SPI_CS_DONE (1 << 16)
#define SPI_CS_TE_EN (1 << 15)
#define SPI_CS_LMONO (1 << 14)
#define SPI_CS_LEN (1 << 13)
#define SPI_CS_REN (1 << 12)
#define SPI_CS_ADCS (1 << 11)
#define SPI_CS_INTR (1 << 10)
#define SPI_CS_INTD (1 << 9)
#define SPI_CS_DMAEN (1 << 8)
#define SPI_CS_TA (1 << 7)
#define SPI_CS_CSPOL (1 << 6)
#define SPI_CS_CLEAR_RX (1 << 5)
#define SPI_CS_CLEAR_TX (1 << 4)
#define SPI_CS_CPOL (1 << 3)
#define SPI_CS_CPHA (1 << 2)
#define SPI_CS_CS 0x3

#define SPI_CS_PIN 7

void spi_init();

void spi_set_clock(uint32_t divider);

void spi_begin_override_set_ce(int enable);

void spi_end_override_set_ce(int enable);

uint8_t spi_transfer(uint8_t data);

void spi_transfer_buffer(uint8_t *tx, uint8_t *rx, int len);