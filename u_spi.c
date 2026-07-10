#include "u_spi.h"

void gpio_spi_init()
{
	gpio_set_function(SPI_CS_PIN, GPIO_FUNC_ALT0);
	gpio_set_function(9, GPIO_FUNC_ALT0);
	gpio_set_function(10, GPIO_FUNC_ALT0);
	gpio_set_function(11, GPIO_FUNC_ALT0);
}

void spi_init()
{
	gpio_spi_init();

	// SPI0 donanımını başlat
	SPI0_CS = SPI_CS_CLEAR_RX | SPI_CS_CLEAR_TX; // FIFO temizle
	SPI0_CLK = 0x100;							 // Clock ayarla (~1 MHz)
	SPI0_CS = SPI_CS_TA;						 // Transfer aktif
}

void spi_set_clock(uint32_t divider)
{
	SPI0_CLK = divider;
}

void spi_begin_override_set_ce(int enable)
{
	gpio_set_function(SPI_CS_PIN, GPIO_FUNC_OUTPUT); // manuel cs

	if (enable)
	{
		digitalWrite(SPI_CS_PIN, LOW);
	}
	else
	{
		digitalWrite(SPI_CS_PIN, HIGH);
	}
}

void spi_end_override_set_ce(int enable)
{
	if (enable)
	{
		digitalWrite(SPI_CS_PIN, LOW);
	}
	else
	{
		digitalWrite(SPI_CS_PIN, HIGH);
	}

	gpio_set_function(SPI_CS_PIN, GPIO_FUNC_ALT0); // auto cs
}

uint8_t spi_transfer(uint8_t data)
{
	// FIFO doluysa bekleme (kernel bloklamaz, busy-wait minimal)
	while (!(SPI0_CS & SPI_CS_TXD))
		; // boş döngü, kernel’i kilitlemez
	SPI0_FIFO = data;

	// Alınan veri hazır olana kadar bekle
	while (!(SPI0_CS & SPI_CS_RXD))
		;

	return SPI0_FIFO & 0xFF;
}

// Çoklu byte transferi (non-blocking değil, ama kernel kilitlemez)
void spi_transfer_buffer(uint8_t *tx, uint8_t *rx, int len)
{
	for (int i = 0; i < len; i++)
	{
		uint8_t data = tx ? tx[i] : 0xFF;
		uint8_t ret = spi_transfer(data);
		if (rx)
			rx[i] = ret;
	}
}
