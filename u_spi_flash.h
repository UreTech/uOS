// sd_spi.h
#ifndef SD_SPI_H
#define SD_SPI_H

#include "u_spi.h"
#include "u_uart.h"
#include "u_gpio.h"
#include "u_cstr_util.h"

// SD Card Commands
#define CMD0    0   // GO_IDLE_STATE
#define CMD8    8   // SEND_IF_COND
#define CMD9    9   // SEND_CSD
#define CMD10   10  // SEND_CID
#define CMD12   12  // STOP_TRANSMISSION
#define CMD16   16  // SET_BLOCKLEN
#define CMD17   17  // READ_SINGLE_BLOCK
#define CMD18   18  // READ_MULTIPLE_BLOCK
#define CMD24   24  // WRITE_BLOCK
#define CMD25   25  // WRITE_MULTIPLE_BLOCK
#define CMD32   32  // ERASE_WR_BLK_START
#define CMD33   33  // ERASE_WR_BLK_END
#define CMD38   38  // ERASE
#define CMD55   55  // APP_CMD
#define CMD58   58  // READ_OCR
#define ACMD41  41  // SD_SEND_OP_COND

// SD Card Responses
#define R1_READY_STATE          0x00
#define R1_IDLE_STATE           0x01
#define R1_ILLEGAL_COMMAND      0x04

// Data tokens
#define DATA_START_BLOCK        0xFE
#define DATA_START_BLOCK_MULTI  0xFC
#define STOP_TRAN_TOKEN         0xFD
#define DATA_ACCEPTED           0x05

// Card types
#define SD_CARD_TYPE_SD1        1
#define SD_CARD_TYPE_SD2        2
#define SD_CARD_TYPE_SDHC       3

// Error codes
#define SD_OK                   0
#define SD_ERROR_INIT           -1
#define SD_ERROR_TIMEOUT        -2
#define SD_ERROR_READ           -3
#define SD_ERROR_WRITE          -4
#define SD_ERROR_ERASE          -5

typedef struct {
    uint8_t type;           // Card type
    uint32_t capacity;      // Capacity in blocks
    uint64_t size_bytes;    // Size in bytes
    uint16_t block_size;    // Block size (usually 512)
    uint8_t initialized;    // Initialization flag
} sd_card_t;

// Public API functions
int sd_init(void);
int sd_read_block(uint32_t block, uint8_t *buffer);
int sd_write_block(uint32_t block, const uint8_t *buffer);
int sd_read_multiple_blocks(uint32_t block, uint8_t *buffer, uint32_t count);
int sd_write_multiple_blocks(uint32_t block, const uint8_t *buffer, uint32_t count);
int sd_erase_blocks(uint32_t start_block, uint32_t end_block);
uint64_t sd_get_size(void);
uint32_t sd_get_block_count(void);

#endif // SD_SPI_H