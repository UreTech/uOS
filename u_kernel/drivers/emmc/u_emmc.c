#include <emmc/u_emmc.h>
#include <u_uart.h>
#include <u_timer.h>
#include <u_gpio.h>
#include <u_cstr_util.h>

void emmc_soft_reset(){
    *EMMC_SOFT_RESET_CONTROL_REGISTER = EMMC_RESET_ALL_BIT;
    time_point start = get_now();
    while((*EMMC_SOFT_RESET_CONTROL_REGISTER & EMMC_RESET_ALL_BIT)){
        if(tp_to_ms(get_now() - start) > 1000){
            udbP("EMMC ERROR: Reset timeout!");
            break;
        }
    }
}

void _emmc_init_clock_(uint32_t clock_in_hz){
    // clear register
    *EMMC_CLOCK_CONTROL_REGISTER = 0x0000;

    *EMMC_CLOCK_CONTROL_REGISTER &= EMMC_CLOCK_SDCLK_DISABLE_RBIT; // disable first
    *EMMC_CLOCK_CONTROL_REGISTER |= EMMC_CLOCK_INTERNAL_CLK_ENABLE_BIT; // enable internal clock

    // calculate suitable divider
    uint16_t suitable_divider_id = (EMMC_INPUT_CLOCK / (clock_in_hz)) / 2ULL;

    // using 10bit divider
    *EMMC_CLOCK_CONTROL_REGISTER |= ((suitable_divider_id & 0xFF) << 8) | (((suitable_divider_id >> 8) & 0x3) << 6);

    *EMMC_CLOCK_CONTROL_REGISTER &= EMMC_CLOCK_DIVIDED_MODE_RBIT; // divider mode
    *EMMC_CLOCK_CONTROL_REGISTER |= EMMC_CLOCK_PLL_ENABLE_BIT;

    time_point start = get_now();
    while (!(*EMMC_CLOCK_CONTROL_REGISTER & EMMC_CLOCK_INTERNAL_STABLE_BIT))
    {
        if(tp_to_ms(get_now() - start) > 1000){
            udbP("EMMC ERROR: Internal clock stable timeout!");
            break;
        }
    }

    *EMMC_CLOCK_CONTROL_REGISTER |= EMMC_CLOCK_SDCLK_ENABLE_BIT; // enable sd clock
}

void _emmc_set_clock_(uint32_t clock_in_hz){
    // calculate suitable divider
    uint16_t suitable_divider_id = (EMMC_INPUT_CLOCK / (clock_in_hz)) / 2ULL;


    *EMMC_CLOCK_CONTROL_REGISTER &= EMMC_CLOCK_SDCLK_DISABLE_RBIT; // disable first
    *EMMC_CLOCK_CONTROL_REGISTER |= EMMC_CLOCK_INTERNAL_CLK_ENABLE_BIT; // enable internal clock

    // clear register
    *EMMC_CLOCK_CONTROL_REGISTER &= ~((0x3FF) << 6);

    // using 10bit divider
    *EMMC_CLOCK_CONTROL_REGISTER |= ((suitable_divider_id & 0xFF) << 8) | (((suitable_divider_id >> 8) & 0x3) << 6);

    time_point start = get_now();
    while (!(*EMMC_CLOCK_CONTROL_REGISTER & EMMC_CLOCK_INTERNAL_STABLE_BIT))
    {
        if(tp_to_ms(get_now() - start) > 1000){
            udbP("EMMC ERROR: Internal clock stable timeout!");
            break;
        }
    }

    *EMMC_CLOCK_CONTROL_REGISTER |= EMMC_CLOCK_SDCLK_ENABLE_BIT; // enable sd clock
}

void emmc_init(){
    
    emmc_soft_reset();

    // set up power
    uint8_t pow_reg = EMMC_SDBUS_VDD1_ENABLE_BIT;
    if((*(uint32_t*)EMMC_CAPABILITIES_REGISTER & EMMC_CAP_3V3_SUPPORT_BIT)){
        pow_reg |= EMMC_SDBUS_VOLTAGE_3V3_FLAG;
        udbP("SDBUS 3v3");
    }else if((*(uint32_t*)EMMC_CAPABILITIES_REGISTER & EMMC_CAP_3V_SUPPORT_BIT)){
        pow_reg |= EMMC_SDBUS_VOLTAGE_3V_FLAG;
        udbP("SDBUS 3v");
    }else if((*(uint32_t*)EMMC_CAPABILITIES_REGISTER & EMMC_CAP_1V8_SUPPORT_BIT)){
        pow_reg |= EMMC_SDBUS_VOLTAGE_1V8_FLAG;
        udbP("SDBUS 1v8");
    }else{
        udbP("EMMC ERROR: No any supported voltage level found!");
        return;
    }

    *EMMC_POWER_CONTROL_REGISTER = pow_reg;

    delay_ms(10);

    // setup clock (400KHz for intiation)
    _emmc_init_clock_(CLOCK_KHZ(400));
}

typedef enum{
    CMD_RESP_NONE,
    CMD_RESP_136,
    CMD_RESP_48,
    CMD_RESP_48_BUSY,
}_cmd_response_type_;

// 6 bit command
// returns -1 for failure
// returns 0 for success
int _emmc_send_sd_command_(uint8_t command_index, uint32_t argument, bool data_present, bool check_crc, bool check_idx, _cmd_response_type_ cmd_resp_type){
    uint16_t cmd = 0x0;

    command_index &= 0b111111; // clear last 2 bit

    uint32_t busy_flags = EMMC_PRESENT_CMD_BUSY_BIT;

    if(data_present){
        cmd |= EMMC_CMD_DATA_PRESENT_BIT;
        busy_flags |= EMMC_PRESENT_DAT_BUSY_BIT;
    }

    if(check_crc){
        cmd |= EMMC_CMD_CRC_CHECK_BIT;
    }

    if(check_idx){
        cmd |= EMMC_CMD_INDEX_CHECK_BIT;
    }

    switch (cmd_resp_type)
    {
    case CMD_RESP_NONE:
        // none
        break;
    case CMD_RESP_136:
        cmd |= 0b01;
        break;
    case CMD_RESP_48:
        cmd |= 0b10;
        break;
    case CMD_RESP_48_BUSY:
        cmd |= 0b11;
        busy_flags |= EMMC_PRESENT_DAT_BUSY_BIT;
        break;
    default:
        // skip
        break;
    }

    // add command index
    cmd |= (uint16_t)command_index << 8ULL;

    time_point start = get_now();
    while (*EMMC_PRESENT_STATE_REGISTER & (busy_flags))
    {
        if(tp_to_ms(get_now() - start) > 1000){
            udbP("EMMC ERROR: CMD/DAT LINE timeout!");
            return -1;
            break;
        }
    }

    // reset irqs
    *EMMC_INTERRUPT_STATUS_REGISTER = 0xFFFF;
    *EMMC_ERROR_INTERRUPT_STATUS_REGISTER = 0xFFFF;

    // set argument
    *EMMC_COMMAND_ARGUMENT_REGISTER = argument;
    // send command
    *EMMC_COMMAND_REGISTER = cmd;

    // wait command
    start = get_now();
    while(true){
        uint16_t irq_status = *EMMC_INTERRUPT_STATUS_REGISTER;
        if(irq_status & EMMC_IRQ_CMD_COMPLETE_BIT){
            // success
            *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_CMD_COMPLETE_BIT; // clear irq
            return 0;
        }

        if(irq_status & EMMC_IRQ_ERROR_BIT){
            *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_ERROR_BIT; // clear irq
            if(*EMMC_ERROR_INTERRUPT_STATUS_REGISTER & EMMC_ERR_IRQ_CMD_TIMEOUT_BIT){
                // timeout fail
                udbP("EMMC ERROR: Command irq soft timeout!");
                *EMMC_ERROR_INTERRUPT_STATUS_REGISTER = EMMC_ERR_IRQ_CMD_TIMEOUT_BIT; // clear irq
                return -1;
            }else{
                // other failure
                udbP("EMMC ERROR: Command irq soft error!");
                uart_print("ERROR IRQ: ");
                uart_print_hex32(*EMMC_ERROR_INTERRUPT_STATUS_REGISTER);
                uart_print("\n");
                *EMMC_ERROR_INTERRUPT_STATUS_REGISTER = ~(0x0); // clear all irq's
                return -1;
            }
        }

        if(tp_to_ms(get_now() - start) > 2500){
            udbP("EMMC ERROR: Command irq hard timeout!");

            uart_print("ERROR IRQ: ");
            uart_print_hex32(*EMMC_ERROR_INTERRUPT_STATUS_REGISTER);
            uart_print("\n");

            return -1;
        }
    }
    
}

// util
#define CARD_STATUS_OUT_OF_RANGE       ONEBIT(31)
#define CARD_STATUS_ADDRESS_ERROR      ONEBIT(30)
#define CARD_STATUS_BLOCK_LEN_ERROR    ONEBIT(29)
#define CARD_STATUS_ERASE_SEQ_ERROR    ONEBIT(28)
#define CARD_STATUS_ERASE_PARAM        ONEBIT(27)
#define CARD_STATUS_WP_VIOLATION       ONEBIT(26)  // write protect!
#define CARD_STATUS_CARD_IS_LOCKED     ONEBIT(25)
#define CARD_STATUS_LOCK_UNLOCK_FAILED ONEBIT(24)
#define CARD_STATUS_COM_CRC_ERROR      ONEBIT(23)
#define CARD_STATUS_ILLEGAL_COMMAND    ONEBIT(22)
#define CARD_STATUS_CARD_ECC_FAILED    ONEBIT(21)
#define CARD_STATUS_CC_ERROR           ONEBIT(20)
#define CARD_STATUS_ERROR              ONEBIT(19)


bool check_r1_response(uint32_t r0){
    uint32_t error_mask = CARD_STATUS_OUT_OF_RANGE | CARD_STATUS_ADDRESS_ERROR |
                          CARD_STATUS_BLOCK_LEN_ERROR | CARD_STATUS_WP_VIOLATION |
                          CARD_STATUS_COM_CRC_ERROR | CARD_STATUS_ILLEGAL_COMMAND |
                          CARD_STATUS_CARD_ECC_FAILED | CARD_STATUS_CC_ERROR |
                          CARD_STATUS_ERROR;
    if(r0 & error_mask){
        uart_print("EMMC ERROR: Card status = 0x");
        uart_print_hex64(r0);
        uart_print("\n");
        return false;
    }
    return true;
}

void wait_r1busy(){
    time_point start = get_now();
    while (!(*EMMC_PRESENT_STATE_REGISTER & EMMC_PRESENT_DAT0_HIGH_BIT))
    {
        if(tp_to_ms(get_now() - start) > 1000){
            udbP("EMMC ERROR: DAT0 busy timeout!");
            break;
        }
    }
}

// little helper for stupid offsets of CSD & CID
uint64_t read_bit_ofsset_with_length(uint8_t* arr, size_t bit_offset, uint8_t length){
    uint64_t result = 0;

    for(size_t i = 0; i < length; i++)
    {
        size_t bit = bit_offset + i;

        size_t byte_offset = bit / 8;
        uint8_t bit_in_byte = 7 - (bit % 8);

        uint8_t value = (arr[byte_offset] >> bit_in_byte) & 1;

        result = (result << 1) | value;
    }

    return result;
}

_sd_card_identification_struct_ _SD_CID_PARSER_(uint8_t* _response){
    // there is some mess going on so try to understand your self :P
    // nutshell: using upper bit offsets from end
    _sd_card_identification_struct_ result;
    result.serial_number = read_bit_ofsset_with_length(_response, (135 - 55), 32);
    result.product_revision = read_bit_ofsset_with_length(_response, (135 - 63), 8);

    for(int i = 0; i < 5; i++){
        result.product_name[i] = read_bit_ofsset_with_length(_response, (135 - 103) + (i * 8), 8);
    }
    result.product_name[5] = '\0'; // add null terminator
    for(int i = 0; i < 2; i++){
        result.oem_id[i] = read_bit_ofsset_with_length(_response, (135 - 119) + (i * 8), 8);
    }
    result.oem_id[2] = '\0'; // add null terminator
    
    result.manufacturer_id = read_bit_ofsset_with_length(_response, (135 - 127), 8);

    return result;
}

_sd_card_csd_ _SD_CSD_PARSER_(uint8_t* _response){
    // there is some mess going on so try to understand your self :P
    // nutshell: using upper bit offsets from end
    _sd_card_csd_ result = {};

    result.csd_version = read_bit_ofsset_with_length(_response, (135 - 127), 2); // total 2bit (0 = v1, 1 = v2, 2 = v3)

    if(result.csd_version == 3){
        return result; // this should not happen but if it happens it returns empty result with invalid version
    }

    if(result.csd_version == 0){
        // V1
        result.erase_sector_size = read_bit_ofsset_with_length(_response, (135 - 45), 7) + 1; // total 7bit (+1 according to sd specifications)
        result.max_read_data_block_length = (size_t)(0b1 << read_bit_ofsset_with_length(_response, (135 - 83), 4)); // block length is calculted by X's power of 2
        
        uint32_t _device_size = read_bit_ofsset_with_length(_response, (135 - 73), 12); // total 12bit
        uint32_t _device_size_multiplier = read_bit_ofsset_with_length(_response, (135 - 49), 3); // total 3bit
        size_t block_count = (_device_size + 1) * (0b1 << (_device_size_multiplier + 2)); // according to sd specifications, block count is calculated by (_device_size + 1) * (2^(_device_size_multiplier + 2))
        result.device_size = block_count * result.max_read_data_block_length;

        return result;
    }else if(result.csd_version == 1){
        // V2
        result.erase_sector_size = read_bit_ofsset_with_length(_response, (135 - 45), 7) + 1; // total 7bit (+1 according to sd specifications)
        result.max_read_data_block_length = (size_t)(0b1 << read_bit_ofsset_with_length(_response, (135 - 83), 4)); // block length is calculted by X's power of 2
        
        uint64_t _device_size = read_bit_ofsset_with_length(_response, (135 - 69), 22); // total 22bit
        result.device_size = (_device_size + 1) * 512 * 1024; // according to sd specifications, total size is calculated by (_device_size + 1) * 512KByte

        return result;
    }else if(result.csd_version == 2){
        // V3
        result.erase_sector_size = read_bit_ofsset_with_length(_response, (135 - 45), 7) + 1; // total 7bit (+1 according to sd specifications)
        result.max_read_data_block_length = (size_t)(0b1 << read_bit_ofsset_with_length(_response, (135 - 83), 4)); // block length is calculted by X's power of 2
        
        uint64_t _device_size = read_bit_ofsset_with_length(_response, (135 - 75), 28); // total 28bit
        result.device_size = (_device_size + 1) * 512 * 1024; // according to sd specifications, total size is calculated by (_device_size + 1) * 512KByte

        return result;
    }

    return result; // this should not happen
}

// OCR register flag bits
#define EMMC_OCR_READY_BIT ONEBIT(31)
#define EMMC_OCR_HIGH_CAPACITY_BIT ONEBIT(30)

// rca for current card
uint16_t current_rca = 0x0;
_sd_card_csd_ csd;
_sd_card_identification_struct_ cid;

_sd_card_csd_ emmc_get_current_csd(){
    return csd;
}

_sd_card_identification_struct_ emmc_get_current_cid(){
    return cid;
}

bool emmc_wait_card_transfer_state(){
    time_point start = get_now();
    while(tp_to_ms(get_now() - start) < 2000){
        if(_emmc_send_sd_command_(SD_CMD(13), (current_rca << 16), false, true, true, CMD_RESP_48) == 0){
            uint32_t status = *EMMC_RESPONSE0_REGISTER;
            uint32_t current_state = (status >> 9) & 0xF;
            if(current_state == 4){ // 4 = "transfer" state
                return true;
            }
        }
    }
    udbP("EMMC ERROR: Card did not return to tran state after operation!");
    return false;
}

// returns EMMC_FAIL or EMMC_SUCCESS
int emmc_init_sd_card(){
    *EMMC_INTERRUPT_STATUS_ENABLE_REGISTER |= EMMC_IRQ_CMD_COMPLETE_ENABLE_BIT | EMMC_IRQ_TRANSFER_COMPLETE_ENABLE_BIT | EMMC_IRQ_DMA_ENABLE_BIT; // enable irqs
    *EMMC_ERROR_INTERRUPT_STATUS_ENABLE_REGISTER = 0xFFFF; // enable all of error interrupts

    _emmc_set_clock_(CLOCK_KHZ(400));

    // set idle / reset
    if(_emmc_send_sd_command_(SD_CMD(0), 0x0, false, false, false, CMD_RESP_NONE)){
        udbP("EMMC SD ERROR: CMD0 failed!");
        return EMMC_FAIL;
    }

    // check sdc v2
    if(_emmc_send_sd_command_(SD_CMD(8), 0x1AA, false, true, true, CMD_RESP_48)){
        udbP("EMMC SD ERROR: CMD8 failed!");
        // return EMMC_FAIL;
    }

    // send ACMD41 until EMMC_OCR_READY_BIT goes high
    time_point start = get_now();
    while (true)
    {
        // send CMD55 first
        if(_emmc_send_sd_command_(SD_CMD(55), 0x0, false, false, false, CMD_RESP_48)){
            udbP("EMMC SD ERROR: CMD55 failed!");
            return EMMC_FAIL;
        }
        // then send CMD41 with HCS argument
        if(_emmc_send_sd_command_(SD_CMD(41), (1u << 30) | 0x00FF8000, false, false, false, CMD_RESP_48)){
            udbP("EMMC SD ERROR: CMD41 failed!");
            return EMMC_FAIL;
        }

        // then check OCR response
        uint32_t resp = *EMMC_RESPONSE0_REGISTER;
        if(resp & EMMC_OCR_READY_BIT){
            if(resp & EMMC_OCR_HIGH_CAPACITY_BIT){
                udbP("EMMC INFO: High capacity card.");
            }else{
                udbP("EMMC INFO: Low capacity card.");
            }

            uart_print("EMMC Card wake up in ");
            uart_print_dec(tp_to_ms(get_now() - start));
            uart_print("ms\n");

            break; // success exit the loop
        }

        if(tp_to_ms(get_now() - start) > 1000){
            udbP("EMMC ERROR: ACMD41 busy timeout!");
            return EMMC_FAIL;
        }
    }
    
    // get CID
    if(_emmc_send_sd_command_(SD_CMD(2), 0x0, false, true, false, CMD_RESP_136)){
        udbP("EMMC SD ERROR: CMD2 failed!");
        return EMMC_FAIL;
    }
    uint32_t regs[4];
    
    // convert big endian
    regs[0] = (*EMMC_RESPONSE0_REGISTER);
    regs[1] = (*EMMC_RESPONSE1_REGISTER);
    regs[2] = (*EMMC_RESPONSE2_REGISTER);
    regs[3] = (*EMMC_RESPONSE3_REGISTER);
    uint8_t cid_bytes[16];
    for(int i = 0; i < 4; i++){
        uint32_t word = regs[3-i];
        cid_bytes[i*4+0] = (word >> 24) & 0xFF;
        cid_bytes[i*4+1] = (word >> 16) & 0xFF;
        cid_bytes[i*4+2] = (word >> 8) & 0xFF;
        cid_bytes[i*4+3] = word & 0xFF;
    }
    
    // parse cid
    cid = _SD_CID_PARSER_(cid_bytes);

    uart_print("CID oem id: ");
    uart_print(cid.oem_id);
    uart_print("\nCID Product Name: ");
    uart_print(cid.product_name);
    uart_print("\n");

    // get RCA
    if(_emmc_send_sd_command_(SD_CMD(3), 0x0, false, true, true, CMD_RESP_48)){
        udbP("EMMC SD ERROR: CMD3 failed!");
        return EMMC_FAIL;
    }
    current_rca = (*EMMC_RESPONSE0_REGISTER >> 16) & 0xFFFF;

    // get CSD
    if(_emmc_send_sd_command_(SD_CMD(9), (current_rca << 16), false, true, false, CMD_RESP_136)){
        udbP("EMMC SD ERROR: CMD9 failed!");
        return EMMC_FAIL;
    }

    // convert big endian
    regs[0] = *EMMC_RESPONSE0_REGISTER;
    regs[1] = *EMMC_RESPONSE1_REGISTER;
    regs[2] = *EMMC_RESPONSE2_REGISTER;
    regs[3] = *EMMC_RESPONSE3_REGISTER;
    uint8_t csd_bytes[16];
    for(int i = 0; i < 4; i++){
        uint32_t word = regs[3-i];
        csd_bytes[i*4+0] = (word >> 24) & 0xFF;
        csd_bytes[i*4+1] = (word >> 16) & 0xFF;
        csd_bytes[i*4+2] = (word >> 8) & 0xFF;
        csd_bytes[i*4+3] = word & 0xFF;
    }

    // parse csd
    csd = _SD_CSD_PARSER_(csd_bytes);

     if(csd.csd_version == 0){
        udbP("EMMC ERROR: SDSC is not supported :(")
        return EMMC_FAIL;
    }else if(csd.csd_version != 3){
        udbP("EMMC INFO: SDHC/SDXC card detected.")
        uart_print("CSD version: ");
        uart_print_dec(csd.csd_version + 1);
        uart_print("\nCard block size: ");
        uart_print_dec(csd.max_read_data_block_length);
        uart_print("\n Card size: ");
        uart_print_dec(csd.device_size);
        uart_print("bytes\n");
    }else{
        udbP("EMMC ERROR: CSD version is invalid!")
        return EMMC_FAIL;
    }

    // setup transfer
    if(_emmc_send_sd_command_(SD_CMD(7), (current_rca << 16), false, true, true, CMD_RESP_48_BUSY)){
        udbP("EMMC SD ERROR: CMD7 failed!");
        return EMMC_FAIL;
    }

    // set block size
    *EMMC_BLOCK_SIZE_REGISTER = EMMC_SDMA_DEFAULT_SDMA_BOUND_FLAG | csd.max_read_data_block_length;
    _emmc_set_clock_(CLOCK_MHZ(25));
    return EMMC_SUCCESS;
}

int emmc_read_sd_card(uint32_t block_index, size_t block_count, uint8_t* buffer){

    uint8_t* orginal_buffer = buffer;
    uint8_t* low_buffer = nullptr;
    if((uint64_t)buffer > SIZE_1G){
        udbP("EMMC SD WARN: EMMC Can not access memory addresses over 1GB! Performing low buffer.");

        // try allocating low buffer
        low_buffer = kmalloc(block_count * 512);
        if(low_buffer == nullptr){
            udbP("EMMC SD ERROR: Low buffer perform failed!");
            return EMMC_FAIL;
        }
        if((uint64_t)low_buffer > SIZE_1G){
            udbP("EMMC SD ERROR: Low buffer perform failed!");
            kfree(low_buffer);
            return EMMC_FAIL;
        }else{
            memcpy(low_buffer, buffer, block_count * 512);
            buffer = low_buffer;
        }
    }

    *EMMC_SDMA_ADDR_REGISTER = ARM_TO_SDMA_BUS_ADDR((uint32_t)buffer);
    *EMMC_BLOCK_COUNT_REGISTER = block_count;  

    *EMMC_INTERRUPT_STATUS_REGISTER = 0xFFFF; // reset irqs

    if(block_count == 1){
        // set flags
        *EMMC_TRANSFER_MODE_REGISTER = EMMC_TRANSFER_MODE_DMA_ENABLE_BIT | EMMC_TRANSFER_MODE_BLOCK_COUNT_ENABLE_BIT | EMMC_TRANSFER_MODE_READ_BIT;

        // read single (CMD17)
        if(_emmc_send_sd_command_(SD_CMD(17), block_index, true, true, true, CMD_RESP_48)){
            udbP("EMMC SD ERROR: CMD17 failed!");
            if(low_buffer != nullptr) kfree(low_buffer);
            return EMMC_FAIL;
        }

        if(!check_r1_response(*EMMC_RESPONSE0_REGISTER)){
            udbP("EMMC SD ERROR: CMD17 failed2!");
            if(low_buffer != nullptr) kfree(low_buffer);
            return EMMC_FAIL;
        }

        // wait sd card & dma
        time_point start = get_now();
        while(true){
            uint16_t irq_status = *EMMC_INTERRUPT_STATUS_REGISTER;

            if(irq_status & EMMC_IRQ_TRANSFER_COMPLETE_BIT){
                *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_TRANSFER_COMPLETE_BIT; // reset irq
                break;
            }

            if(irq_status & EMMC_IRQ_DMA_BIT){
                // dma bound irq
                buffer += EMMC_SDMA_DEFAULT_SDMA_BOUND_SIZE;
                *EMMC_SDMA_ADDR_REGISTER = ARM_TO_SDMA_BUS_ADDR((uint32_t)buffer);
                *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_DMA_BIT; // reset irq
            }

            if(irq_status & EMMC_IRQ_ERROR_BIT){
                *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_ERROR_BIT; // reset irq
                *EMMC_ERROR_INTERRUPT_STATUS_REGISTER = 0xFFFF; // reset error irqs
                udbP("EMMC ERROR: DMA/Single Read error!");
                if(low_buffer != nullptr) kfree(low_buffer);
                return EMMC_FAIL;
            }

            if(tp_to_ms(get_now() - start) > 1000){
                udbP("EMMC ERROR: DMA/Single Read irq timeout!");
                if(low_buffer != nullptr) kfree(low_buffer);
                return EMMC_FAIL;
            }
        }

        emmc_wait_card_transfer_state();
        if(low_buffer != nullptr){
            memcpy(orginal_buffer, low_buffer, block_count * 512);
            kfree(low_buffer);
        }
        return EMMC_SUCCESS;
    }else{
        // set flags
        *EMMC_TRANSFER_MODE_REGISTER = EMMC_TRANSFER_MODE_DMA_ENABLE_BIT | EMMC_TRANSFER_MODE_BLOCK_COUNT_ENABLE_BIT | EMMC_TRANSFER_MODE_READ_BIT | EMMC_TRANSFER_MODE_MULTI_BLOCK_ENABLE_BIT | EMMC_TRANSFER_MODE_AUTO_CMD12_FLAG;

        // read multiple (CMD18)
        if(_emmc_send_sd_command_(SD_CMD(18), block_index, true, true, true, CMD_RESP_48)){
            udbP("EMMC SD ERROR: CMD18 failed!");
            if(low_buffer != nullptr) kfree(low_buffer);
            return EMMC_FAIL;
        }

        if(!check_r1_response(*EMMC_RESPONSE0_REGISTER)){
            udbP("EMMC SD ERROR: CMD18 failed2!");
            if(low_buffer != nullptr) kfree(low_buffer);
            return EMMC_FAIL;
        }

        // wait sd card & dma
        time_point start = get_now();
        while(true){
            uint16_t irq_status = *EMMC_INTERRUPT_STATUS_REGISTER;

            if(irq_status & EMMC_IRQ_TRANSFER_COMPLETE_BIT){
                *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_TRANSFER_COMPLETE_BIT; // reset irq
                break;
            }
            
            if(irq_status & EMMC_IRQ_DMA_BIT){
                // dma bound irq
                buffer += EMMC_SDMA_DEFAULT_SDMA_BOUND_SIZE;
                *EMMC_SDMA_ADDR_REGISTER = ARM_TO_SDMA_BUS_ADDR((uint32_t)buffer);
                *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_DMA_BIT; // reset irq
            }
            
            if(irq_status & EMMC_IRQ_ERROR_BIT){
                *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_ERROR_BIT; // reset irq
                *EMMC_ERROR_INTERRUPT_STATUS_REGISTER = 0xFFFF; // reset error irqs
                udbP("EMMC ERROR: DMA/Multi Read error!");
                if(low_buffer != nullptr) kfree(low_buffer);
                return EMMC_FAIL;
            }

            if(tp_to_ms(get_now() - start) > 1000){
                udbP("EMMC ERROR: DMA/Multi Read irq timeout!");
                if(low_buffer != nullptr) kfree(low_buffer);
                return EMMC_FAIL;
            }
        }

        emmc_wait_card_transfer_state();
        if(low_buffer != nullptr){
            memcpy(orginal_buffer, low_buffer, block_count * 512);
            kfree(low_buffer);
        }
        return EMMC_SUCCESS;
    }
    if(low_buffer != nullptr) kfree(low_buffer);
    return EMMC_FAIL;
}

int emmc_write_sd_card(uint32_t block_index, size_t block_count, uint8_t* buffer){

    uint8_t* orginal_buffer = buffer;
    uint8_t* low_buffer = nullptr;
    if((uint64_t)buffer > SIZE_1G){
        udbP("EMMC SD WARN: EMMC Can not access memory addresses over 1GB! Performing low buffer.");

        // try allocating low buffer
        low_buffer = kmalloc(block_count * 512);
        if(low_buffer == nullptr){
            udbP("EMMC SD ERROR: Low buffer perform failed!");
            return EMMC_FAIL;
        }
        if((uint64_t)low_buffer > SIZE_1G){
            udbP("EMMC SD ERROR: Low buffer perform failed!");
            kfree(low_buffer);
            return EMMC_FAIL;
        }else{
            memcpy(low_buffer, buffer, block_count * 512);
            buffer = low_buffer;
        }
    }

    *EMMC_SDMA_ADDR_REGISTER = ARM_TO_SDMA_BUS_ADDR((uint32_t)buffer);
    *EMMC_BLOCK_COUNT_REGISTER = block_count;   

    *EMMC_INTERRUPT_STATUS_REGISTER = 0xFFFF; // reset irqs

    if(block_count == 1){
        // set flags
        *EMMC_TRANSFER_MODE_REGISTER = EMMC_TRANSFER_MODE_DMA_ENABLE_BIT | EMMC_TRANSFER_MODE_BLOCK_COUNT_ENABLE_BIT;

        // write single (CMD24)
        if(_emmc_send_sd_command_(SD_CMD(24), block_index, true, true, true, CMD_RESP_48_BUSY)){
            udbP("EMMC SD ERROR: CMD24 failed!");
            if(low_buffer != nullptr) kfree(low_buffer);
            return EMMC_FAIL;
        }

        if(!check_r1_response(*EMMC_RESPONSE0_REGISTER)){
            udbP("EMMC SD ERROR: CMD24 failed2!");
            if(low_buffer != nullptr) kfree(low_buffer);
            return EMMC_FAIL;
        }

        // wait sd card & dma
        time_point start = get_now();
        while(true){
            uint16_t irq_status = *EMMC_INTERRUPT_STATUS_REGISTER;

            if(irq_status & EMMC_IRQ_TRANSFER_COMPLETE_BIT){
                *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_TRANSFER_COMPLETE_BIT; // reset irq
                break;
            }

            if(irq_status & EMMC_IRQ_DMA_BIT){
                // dma bound irq
                buffer += EMMC_SDMA_DEFAULT_SDMA_BOUND_SIZE;
                *EMMC_SDMA_ADDR_REGISTER = ARM_TO_SDMA_BUS_ADDR((uint32_t)buffer);
                *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_DMA_BIT; // reset irq
            }

            if(irq_status & EMMC_IRQ_ERROR_BIT){
                *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_ERROR_BIT; // reset irq
                *EMMC_ERROR_INTERRUPT_STATUS_REGISTER = 0xFFFF; // reset error irqs
                udbP("EMMC ERROR: DMA/Single Write error!");
                if(low_buffer != nullptr) kfree(low_buffer);
                return EMMC_FAIL;
            }

            if(tp_to_ms(get_now() - start) > 1000){
                udbP("EMMC ERROR: DMA/Single Write irq timeout!");
                if(low_buffer != nullptr) kfree(low_buffer);
                return EMMC_FAIL;
            }
        }

        wait_r1busy();
        emmc_wait_card_transfer_state();
        if(low_buffer != nullptr) kfree(low_buffer);
        return EMMC_SUCCESS;
    }else{
        // set flags
        *EMMC_TRANSFER_MODE_REGISTER = EMMC_TRANSFER_MODE_DMA_ENABLE_BIT | EMMC_TRANSFER_MODE_BLOCK_COUNT_ENABLE_BIT | EMMC_TRANSFER_MODE_MULTI_BLOCK_ENABLE_BIT | EMMC_TRANSFER_MODE_AUTO_CMD12_FLAG;

        // read multiple (CMD25)
        if(_emmc_send_sd_command_(SD_CMD(25), block_index, true, true, true, CMD_RESP_48_BUSY)){
            udbP("EMMC SD ERROR: CMD25 failed!");
            if(low_buffer != nullptr) kfree(low_buffer);
            return EMMC_FAIL;
        }

        if(!check_r1_response(*EMMC_RESPONSE0_REGISTER)){
            udbP("EMMC SD ERROR: CMD25 failed2!");
            if(low_buffer != nullptr) kfree(low_buffer);
            return EMMC_FAIL;
        }

        // wait sd card & dma
        time_point start = get_now();
        while(true){
            uint16_t irq_status = *EMMC_INTERRUPT_STATUS_REGISTER;

            if(irq_status & EMMC_IRQ_TRANSFER_COMPLETE_BIT){
                *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_TRANSFER_COMPLETE_BIT; // reset irq
                break;
            }
            
            if(irq_status & EMMC_IRQ_DMA_BIT){
                // dma bound irq
                buffer += EMMC_SDMA_DEFAULT_SDMA_BOUND_SIZE;
                *EMMC_SDMA_ADDR_REGISTER = ARM_TO_SDMA_BUS_ADDR((uint32_t)buffer);
                *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_DMA_BIT; // reset irq
            }
            
            if(irq_status & EMMC_IRQ_ERROR_BIT){
                *EMMC_INTERRUPT_STATUS_REGISTER = EMMC_IRQ_ERROR_BIT; // reset irq
                *EMMC_ERROR_INTERRUPT_STATUS_REGISTER = 0xFFFF; // reset error irqs
                udbP("EMMC ERROR: DMA/Multi Write error!");
                if(low_buffer != nullptr) kfree(low_buffer);
                return EMMC_FAIL;
            }

            if(tp_to_ms(get_now() - start) > 1000){
                udbP("EMMC ERROR: DMA/Multi Write irq timeout!");
                if(low_buffer != nullptr) kfree(low_buffer);
                return EMMC_FAIL;
            }
        }

        wait_r1busy();
        emmc_wait_card_transfer_state();
        if(low_buffer != nullptr) kfree(low_buffer);
        return EMMC_SUCCESS;
    }
    if(low_buffer != nullptr) kfree(low_buffer);
    return EMMC_FAIL;
}

