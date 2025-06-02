#include"u_sd_spi.h"
#include"u_ctypes.h"
#include"u_timer.h"
#include"u_uart.h"
#include"u_gpio.h"

#define CMD0    0x40  // GO_IDLE_STATE
#define CMD8    0x48  // SEND_IF_COND
#define CMD17   0x51  // READ_SINGLE_BLOCK
#define CMD24   0x58  // WRITE_SINGLE_BLOCK
#define CMD55   0x77
#define ACMD41  0x69

#define SD_DUMMY_BYTE 0xFF

#define CS_LOW()   spi_chip_select(0) // CE0
#define CS_HIGH()  spi_chip_select(255) // DESELECT

static void sd_send_dummy_clocks() {
    CS_HIGH();
    for (int i = 0; i < 10; ++i)
        spi_send((unsigned char[]){SD_DUMMY_BYTE}, 1);
}

static unsigned char sd_send_command(unsigned char cmd, unsigned int arg, unsigned char crc) {
    unsigned char packet[6];

    packet[0] = cmd;
    packet[1] = (arg >> 24) & 0xFF;
    packet[2] = (arg >> 16) & 0xFF;
    packet[3] = (arg >> 8)  & 0xFF;
    packet[4] = (arg)       & 0xFF;
    packet[5] = crc;

    spi_send(packet, 6);

    // Wait for a response (response starts with MSB=0)
    unsigned char response = 0xFF;
    for (int i = 0; i < 10; i++) {
        spi_recv(&response, 1);
        if (response != 0xFF) break;
    }

    return response;
}

int sd_init() {
    spi_init();
    sd_send_dummy_clocks();

    CS_LOW();

    // CMD0: GO_IDLE_STATE
    if (sd_send_command(CMD0, 0, 0x95) != 0x01) {
        CS_HIGH();
        return -1;
    }

    // CMD8: SEND_IF_COND (check voltage range)
    unsigned char resp[5] = {0};
    spi_send((unsigned char[]){CMD8, 0, 0, 0x01, 0xAA, 0x87}, 6);
    for (int i = 0; i < 5; ++i) {
        spi_recv(&resp[i], 1);
        if (!(resp[0] & 0x80)) break;
    }

    // ACMD41 loop (init)
    int retry = 1000;
    do {
        sd_send_command(CMD55, 0, 0x65);
        if (sd_send_command(ACMD41, 0x40000000, 0x77) == 0x00)
            break;
    } while (--retry);

    CS_HIGH();
    spi_send((unsigned char[]){SD_DUMMY_BYTE}, 1);
    return (retry == 0) ? -2 : 0;
}

int sd_read_sector(unsigned int sector, unsigned char *buffer) {
    CS_LOW();

    unsigned char r = sd_send_command(CMD17, sector * 512, 0xFF);
    if (r != 0x00) {
        CS_HIGH();
        return -1;
    }

    // Wait for start token (0xFE)
    unsigned char token = 0xFF;
    for (int i = 0; i < 10000; ++i) {
        spi_recv(&token, 1);
        if (token == 0xFE) break;
    }
    if (token != 0xFE) {
        CS_HIGH();
        return -2;
    }

    // Read 512 bytes
    spi_recv(buffer, 512);

    // Read and discard CRC
    unsigned char dummy[2];
    spi_recv(dummy, 2);

    CS_HIGH();
    spi_send((unsigned char[]){SD_DUMMY_BYTE}, 1);
    return 0;
}

int sd_write_sector(unsigned int sector, const unsigned char *buffer) {
    CS_LOW();

    if (sd_send_command(CMD24, sector * 512, 0xFF) != 0x00) {
        CS_HIGH();
        return -1;
    }

    // Send start token
    spi_send((unsigned char[]){0xFE}, 1);

    // Write 512 bytes
    spi_send((unsigned char*)buffer, 512);

    // Send dummy CRC
    spi_send((unsigned char[]){0xFF, 0xFF}, 2);

    // Read data response
    unsigned char resp;
    spi_recv(&resp, 1);
    if ((resp & 0x1F) != 0x05) {
        CS_HIGH();
        return -2;
    }

    // Wait for write complete
    do {
        spi_recv(&resp, 1);
    } while (resp == 0);

    CS_HIGH();
    spi_send((unsigned char[]){SD_DUMMY_BYTE}, 1);
    return 0;
}
