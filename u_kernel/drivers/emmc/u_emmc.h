#ifndef u_EMMC_H
#define u_EMMC_H

// UreTech eMMC driver header

#include <u_kernel/util/u_ctypes.h>

// EMMC base mmio address for rpi4
#define EMMC_BASE 0xFE340000UL

#define EMMC_INPUT_CLOCK 100000000ULL // 100MHz

#define EMMC_SDMA_ADDR_REGISTER ((volatile uint32_t *)(EMMC_BASE + 0x0)) // pysical address for SDMA
#define EMMC_ARG2_REGISTER EMMC_SDMA_ADDR_REGISTER
#define EMMC_BLOCK_SIZE_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0x4))
#define EMMC_BLOCK_COUNT_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0x6))
#define EMMC_COMMAND_ARGUMENT_REGISTER ((volatile uint32_t *)(EMMC_BASE + 0x8))
#define EMMC_TRANSFER_MODE_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0xC))
#define EMMC_COMMAND_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0xE))
#define EMMC_RESPONSE0_REGISTER ((volatile uint32_t *)(EMMC_BASE + 0x10))
#define EMMC_RESPONSE1_REGISTER ((volatile uint32_t *)(EMMC_BASE + 0x14))
#define EMMC_RESPONSE2_REGISTER ((volatile uint32_t *)(EMMC_BASE + 0x18))
#define EMMC_RESPONSE3_REGISTER ((volatile uint32_t *)(EMMC_BASE + 0x1C))
#define EMMC_BUFFER_DATA_PORT_REGISTER ((volatile uint32_t *)(EMMC_BASE + 0x20))
#define EMMC_PRESENT_STATE_REGISTER ((volatile uint32_t *)(EMMC_BASE + 0x24))
#define EMMC_HOST_CONTROL1_REGISTER ((volatile uint8_t *)(EMMC_BASE + 0x28))
#define EMMC_POWER_CONTROL_REGISTER ((volatile uint8_t *)(EMMC_BASE + 0x29))
#define EMMC_BLOCK_GAP_CONTROL_REGISTER ((volatile uint8_t *)(EMMC_BASE + 0x2A))
#define EMMC_WAKEUP_CONTROL_REGISTER ((volatile uint8_t *)(EMMC_BASE + 0x2B))
#define EMMC_CLOCK_CONTROL_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0x2C))
#define EMMC_TIMEOUT_CONTROL_REGISTER ((volatile uint8_t *)(EMMC_BASE + 0x2E))
#define EMMC_SOFT_RESET_CONTROL_REGISTER ((volatile uint8_t *)(EMMC_BASE + 0x2F))
#define EMMC_INTERRUPT_STATUS_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0x30))
#define EMMC_ERROR_INTERRUPT_STATUS_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0x32))
#define EMMC_INTERRUPT_STATUS_ENABLE_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0x34))
#define EMMC_ERROR_INTERRUPT_STATUS_ENABLE_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0x36))
#define EMMC_INTERRUPT_SIGNAL_ENABLE_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0x38))
#define EMMC_ERROR_INTERRUPT_SIGNAL_ENABLE_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0x3A))
#define EMMC_AUTO_COMMAND_ERROR_STATUS_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0x3C))
#define EMMC_HOST_CONTROL2_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0x3E))
#define EMMC_CAPABILITIES_REGISTER ((volatile uint64_t *)(EMMC_BASE + 0x40))
#define EMMC_MAXIMUM_CURRENT_CAPABILITIES_REGISTER ((volatile uint64_t *)(EMMC_BASE + 0x48))
#define EMMC_FORCE_EVENT_INTERRUPT_REGISTER ((volatile uint16_t *)(EMMC_BASE + 0x50))

// reset bits
#define EMMC_RESET_ALL_BIT ONEBIT(0)
#define EMMC_RESET_CMD_LINE_BIT ONEBIT(1)
#define EMMC_RESET_DAT_LINE_BIT ONEBIT(2)

// clock control flag bits
#define EMMC_CLOCK_PROGRAMMABLE_MODE_BIT ONEBIT(5)
#define EMMC_CLOCK_DIVIDED_MODE_RBIT ~ONEBIT(5) 
#define EMMC_CLOCK_PLL_ENABLE_BIT ONEBIT(3)
#define EMMC_CLOCK_PLL_DISABLE_RBIT ~ONEBIT(3)
#define EMMC_CLOCK_SDCLK_ENABLE_BIT ONEBIT(2)
#define EMMC_CLOCK_SDCLK_DISABLE_RBIT ~ONEBIT(2)

#define EMMC_CLOCK_INTERNAL_STABLE_BIT ONEBIT(1)

#define EMMC_CLOCK_INTERNAL_CLK_ENABLE_BIT ONEBIT(0)
#define EMMC_CLOCK_INTERNAL_CLK_DISABLE_BIT ~ONEBIT(0)

// capablities flag bits
#define EMMC_CAP_3V3_SUPPORT_BIT  ONEBIT(24)
#define EMMC_CAP_3V_SUPPORT_BIT  ONEBIT(25)
#define EMMC_CAP_1V8_SUPPORT_BIT  ONEBIT(26)

// power control register bits
#define EMMC_SDBUS_VOLTAGE_3V3_FLAG (0b111 << 1ULL)
#define EMMC_SDBUS_VOLTAGE_3V_FLAG (0b110 << 1ULL)
#define EMMC_SDBUS_VOLTAGE_1V8_FLAG (0b101 << 1ULL)
#define EMMC_SDBUS_VDD1_ENABLE_BIT ONEBIT(0)
#define EMMC_SDBUS_VDD1_DISABLE_BIT ~ONEBIT(0)

// present state register flag bits
#define EMMC_PRESENT_DAT0_HIGH_BIT ONEBIT(20)
#define EMMC_PRESENT_CARD_DETECT_PIN_LEVEL_BIT ONEBIT(18)
#define EMMC_PRESENT_CARD_INSERTED_BIT ONEBIT(16)
#define EMMC_PRESENT_CMD_BUSY_BIT ONEBIT(0)
#define EMMC_PRESENT_DAT_BUSY_BIT ONEBIT(1)

// command register flag bits
#define EMMC_CMD_DATA_PRESENT_BIT ONEBIT(5)
#define EMMC_CMD_INDEX_CHECK_BIT ONEBIT(4)
#define EMMC_CMD_CRC_CHECK_BIT ONEBIT(3)

// interrupt status flag bits
#define EMMC_IRQ_CMD_COMPLETE_BIT ONEBIT(0)
#define EMMC_IRQ_TRANSFER_COMPLETE_BIT ONEBIT(1)
#define EMMC_IRQ_DMA_BIT ONEBIT(3)
#define EMMC_IRQ_ERROR_BIT ONEBIT(15)

// error interrupt status flag bits
#define EMMC_ERR_IRQ_CMD_TIMEOUT_BIT ONEBIT(0)

// interrupt enable flag bits
#define EMMC_IRQ_CMD_COMPLETE_ENABLE_BIT ONEBIT(0)
#define EMMC_IRQ_TRANSFER_COMPLETE_ENABLE_BIT ONEBIT(1)
#define EMMC_IRQ_DMA_ENABLE_BIT ONEBIT(3)

// error interrupt enable flag bits
#define EMMC_ERR_IRQ_CMD_TIMEOUT_ENABLE_BIT ONEBIT(0)

// transfer mode flag bits
#define EMMC_TRANSFER_MODE_DMA_ENABLE_BIT ONEBIT(0)
#define EMMC_TRANSFER_MODE_BLOCK_COUNT_ENABLE_BIT ONEBIT(1)
#define EMMC_TRANSFER_MODE_READ_BIT ONEBIT(4) // 0 is write
#define EMMC_TRANSFER_MODE_MULTI_BLOCK_ENABLE_BIT ONEBIT(5)
#define EMMC_TRANSFER_MODE_AUTO_CMD12_FLAG (0b01 << 2)
#define EMMC_TRANSFER_MODE_AUTO_CMD23_FLAG (0b10 << 2)
#define EMMC_TRANSFER_MODE_AUTO_CMD_AUTO_SELECT_FLAG (0b11 << 2)

// SDMA Buffer Boundary flags
#define EMMC_SDMA_BOUND_4K_FLAG (0b000 << 12)
#define EMMC_SDMA_BOUND_8K_FLAG (0b001 << 12)
#define EMMC_SDMA_BOUND_16K_FLAG (0b010 << 12)
#define EMMC_SDMA_BOUND_32K_FLAG (0b011 << 12)
#define EMMC_SDMA_BOUND_64K_FLAG (0b100 << 12)
#define EMMC_SDMA_BOUND_128K_FLAG (0b101 << 12)
#define EMMC_SDMA_BOUND_256K_FLAG (0b110 << 12)
#define EMMC_SDMA_BOUND_512K_FLAG (0b111 << 12)

#define EMMC_SDMA_DEFAULT_SDMA_BOUND_FLAG EMMC_SDMA_BOUND_512K_FLAG
#define EMMC_SDMA_DEFAULT_SDMA_BOUND_SIZE (512ULL * 1024ULL)

// util
#define SD_CMD(idx) (idx)
#define EMMC_FAIL false
#define EMMC_SUCCESS true

// THIS IS MACRO IS MOST IMPORTANT THING HERE!
#define ARM_TO_SDMA_BUS_ADDR(addr) ((uint32_t)(addr) + 0xC0000000UL)

// CID "Pretty version"
// NOTE: This structure is not correctly offseted or paded with response register offsets
// use with _SD_CID_PARSER_()
typedef struct{
    uint8_t  product_revision;
    char     product_name[5 + 1]; // 40 bit wide + null terminator
    char     oem_id[2 + 1]; // 16 bit wide + null terminator
    uint8_t  manufacturer_id;
    uint32_t serial_number;
}__attribute__((aligned(16))) _sd_card_identification_struct_;
_sd_card_identification_struct_ _SD_CID_PARSER_(uint8_t* _response); // WARNING! Only 16 byte (4 x 32 bit response register) is allowed!

// CSD (Card Specific Data) "Pretty version"
// NOTE: This structure is not correctly offseted or paded with response register offsets
// use with _SD_CSD_PARSER_()
typedef struct
{
    uint64_t max_read_data_block_length; // [v1, v2, v3]
    uint64_t device_size; // [v1, v2, v3]
    uint64_t erase_sector_size; // [v1, v2, v3]   
    uint8_t csd_version; // version of this structure [v1, v2, v3] (this put to end for not blowing up alginment fault)
    uint8_t padding[7];
}__attribute__((aligned(16))) _sd_card_csd_;
_sd_card_csd_ _SD_CSD_PARSER_(uint8_t* _response); // WARNING! Only 16 byte (4 x 32 bit response register) is allowed!

typedef struct
{
    _sd_card_identification_struct_ cid;
    _sd_card_csd_ csd;

}_sd_card_info_;



void emmc_soft_reset();

void emmc_init();

int emmc_init_sd_card();

_sd_card_csd_ emmc_get_current_csd();
_sd_card_identification_struct_ emmc_get_current_cid();

int emmc_read_sd_card(uint32_t block_index, size_t block_count, uint8_t* buffer);
int emmc_write_sd_card(uint32_t block_index, size_t block_count, uint8_t* buffer);

#endif