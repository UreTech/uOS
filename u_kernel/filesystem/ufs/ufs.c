#include <u_kernel/filesystem/ufs/ufs.h>
#include <u_kernel/util/u_ctypes.h>
#include <u_kernel//memory/u_memory.h>
#include <u_kernel/drivers/emmc/u_emmc.h>
#include <u_kernel/util/util.h>
#include <u_kernel/util/random/u_rand.h>
#include <u_kernel/util/u_cstr_util.h>
#include <u_kernel/timer/u_timer.h>

#pragma pack(push,1) // disable alignment (maybe dangerous?)
typedef struct
{
    uint8_t boot_indicator; // 0x80 bootable, other not
    uint8_t starting_chs[3];
    uint8_t os_type;
    uint8_t ending_chs[3];
    uint32_t starting_lba;
    uint32_t size_in_lba;
}MBR_PARTITION_RECORD;

typedef struct
{
    uint8_t boot_code[440]; // (offset 0)
    uint32_t UniqueMBRDiskSignature; // (offset 440)
    uint8_t unknown[2]; // (offset 444)
    MBR_PARTITION_RECORD partition_record[4]; // (offset 446)
    uint16_t signature; // (offset 510)
    // empty...
}MBR_HEADER;

typedef struct
{
    uint64_t signature; // (offset 0) always 0x5452415020494645 "EFI PART"
    uint32_t revision; // (offset 8) always 0x10000
    uint32_t header_size; // (offset 12)
    uint32_t header_crc32; // (offset 16)
    uint32_t reserved; // (offset 20)
    uint64_t self_lba; // (offset 24)
    uint64_t alternate_lba; // (offset 32)
    uint64_t first_usable_lba; // (offset 40)
    uint64_t last_usable_lba; // (offset 48)
    uint64_t disk_guid[2]; // (offset 56)
    uint64_t partition_entry_lba; // (offset 72)
    uint32_t number_of_partition_entries; // (offset 80)
    uint32_t size_of_partition_entry; // (offset 84)
    uint32_t partition_entry_array_crc32; // (offset 88)
    // empty...
    char empty[512 - 92];
}GPT_HEADER;

typedef struct
{
    uint64_t partition_type_guid[2]; // (offset 0) 0 for not used entry
    uint64_t unique_partition_guid[2]; // (offset 16)
    uint64_t starting_lba; // (offset 32)
    uint64_t ending_lba; // (offset 40)
    uint64_t attributes; // (offset 48)
    uint16_t patrtition_name[36]; // (offset 56) null terminator included
    // total size 128
}GPT_PARTITION_ENTRY; 

#pragma pack(pop)

void char_to_utf16_buf(const char* _char, uint16_t* _wchar){
    size_t len = strlen(_char, 10000) + 1; // include null terminator
    for(size_t i = 0; i < len; i++){
        _wchar[i] = (uint16_t)(unsigned char)_char[i];
    }
}

int format_sd_gpt_with_pre_partitions(){
    uint64_t total_lba_count = emmc_get_current_csd().device_size / emmc_get_current_csd().max_read_data_block_length;
    uart_print("total size: ");
    uart_print_dec(emmc_get_current_csd().device_size);
    uart_print("\nblock size: ");
    uart_print_dec(emmc_get_current_csd().max_read_data_block_length);
    uart_print("\nblock count: ");
    uart_print_dec(total_lba_count);
    uart_print("\n");
    // protective mbr required for GPT
    MBR_HEADER protective_mbr;
    memset(protective_mbr.boot_code, 0x0, sizeof(protective_mbr.boot_code));
    protective_mbr.UniqueMBRDiskSignature = 0x0;
    protective_mbr.unknown[0] = 0x0;
    protective_mbr.unknown[1] = 0x0;
    protective_mbr.signature = 0xAA55;
    
    protective_mbr.partition_record[0].boot_indicator = 0x0; // not bootable

    protective_mbr.partition_record[0].starting_chs[0] = 0x00;
    protective_mbr.partition_record[0].starting_chs[1] = 0x02;
    protective_mbr.partition_record[0].starting_chs[2] = 0x00;

    protective_mbr.partition_record[0].os_type = 0xEE; // GPT protective type

    protective_mbr.partition_record[0].ending_chs[0] = 0xFF;
    protective_mbr.partition_record[0].ending_chs[1] = 0xFF;
    protective_mbr.partition_record[0].ending_chs[2] = 0xFF;

    // skip header
    uint32_t temp = 1;
    memcpy(((uint8_t*)&(protective_mbr.partition_record[0].starting_lba)) ,&temp,sizeof(temp)); // memcpy for eliminating alignment faults

    if(total_lba_count < 0xFFFFFFFF){
        temp = total_lba_count - 1; // set size
    }else{
        temp = 0xFFFFFFFF; // set max size
    }
    memcpy(((uint8_t*)&(protective_mbr.partition_record[0].size_in_lba)) ,&temp,sizeof(temp));

    memset(&(protective_mbr.partition_record[1]), 0x0, sizeof(MBR_PARTITION_RECORD) * 3); // zero other entries

    // write protective mbr to lba0
    udb();

    if(emmc_write_sd_card(0, 1, (uint8_t*)(&protective_mbr)) == EMMC_FAIL){
        udbP("UFS GPT ERROR: Failed to write protective mbr header!");
        return UFS_FAIL;
    }

    udb();

    // prepare gpt header
    GPT_HEADER gpt_header = {0};

    gpt_header.signature = 0x5452415020494645; // "EFI PART"
    gpt_header.revision = 0x10000;
    gpt_header.header_size = 92ULL;
    gpt_header.alternate_lba = total_lba_count - 1; // last lba
    gpt_header.first_usable_lba = 34ULL; // (1lb offset) 1 gpt header + (128 / 4) 32 entry = 34th lba
    gpt_header.last_usable_lba = total_lba_count - 34; // 34rd lba from end
    udb();
    gpt_header.disk_guid[0] = hardware_rng64();
    udb();
    gpt_header.disk_guid[1] = hardware_rng64();
    udb();

    gpt_header.self_lba = 1ULL; // this lba
    udb();
    gpt_header.partition_entry_lba = 2ULL;
    gpt_header.number_of_partition_entries = 128;
    gpt_header.size_of_partition_entry = 128;
    udb();
    gpt_header.partition_entry_array_crc32 = 0ULL; // set later
    gpt_header.header_crc32 = 0ULL; // set later

    // save for future
    uint64_t alternative_header_lba = gpt_header.alternate_lba;
    uint64_t alternative_entry_lba = gpt_header.alternate_lba - 32;

    GPT_PARTITION_ENTRY block_of_entries[4]; // 1 lba
    memset(block_of_entries, 0x0, sizeof(GPT_PARTITION_ENTRY) * 4); // zero

    udb();

    // write empty 31 entry
    for(uint32_t i = 1; i < 32; i++){
        // default
        if(emmc_write_sd_card(gpt_header.partition_entry_lba + i, 1, (uint8_t*)(&block_of_entries)) == EMMC_FAIL){
            udbP("UFS GPT ERROR: Failed to write GPT partition entry!");
            return UFS_FAIL;
        }

        // alternative
        if(emmc_write_sd_card(alternative_entry_lba + i, 1, (uint8_t*)(&block_of_entries)) == EMMC_FAIL){
            udbP("UFS GPT ERROR: Failed to write GPT partition entry!");
            return UFS_FAIL;
        }
    }

    udb();

    // prepare preset partitions

    uint64_t first_partitionable_lba = 34;
    size_t not_allocated_lba_count = total_lba_count - 66; // 33 default + 33 alternative

    // FAT for bootloader
    char_to_utf16_buf("uBOOT", block_of_entries[0].patrtition_name);
    block_of_entries[0].partition_type_guid[0] = 0x11D2F81FC12A7328ULL; // esp
    block_of_entries[0].partition_type_guid[1] = 0x3BC93EC9A0004BBAULL; // esp
    block_of_entries[0].unique_partition_guid[0] = hardware_rng64();
    block_of_entries[0].unique_partition_guid[1] = hardware_rng64();
    block_of_entries[0].attributes = 0x0;
    block_of_entries[0].starting_lba = first_partitionable_lba;
    block_of_entries[0].ending_lba = first_partitionable_lba + ((SIZE_1M * 64) / 512) - 1; // 64MB
    first_partitionable_lba += ((SIZE_1M * 64) / 512);
    not_allocated_lba_count -= ((SIZE_1M * 64) / 512);

    // UFS for uOS
    char_to_utf16_buf("uOS-UFS1", block_of_entries[1].patrtition_name);
    block_of_entries[1].partition_type_guid[0] = UFS_GPT_GUID_LOW; // ufs
    block_of_entries[1].partition_type_guid[1] = UFS_GPT_GUID_HIGH; // ufs
    block_of_entries[1].unique_partition_guid[0] = hardware_rng64();
    block_of_entries[1].unique_partition_guid[1] = hardware_rng64();
    block_of_entries[1].attributes = 0x0;
    block_of_entries[1].starting_lba = first_partitionable_lba;
    block_of_entries[1].ending_lba = first_partitionable_lba + not_allocated_lba_count - 1; // remaining
    first_partitionable_lba += not_allocated_lba_count;
    not_allocated_lba_count -= 0;

    udb();

    // write new entries
    // default
    if(emmc_write_sd_card(gpt_header.partition_entry_lba, 1, (uint8_t*)(&block_of_entries)) == EMMC_FAIL){
        udbP("UFS GPT ERROR: Failed to write GPT partition entry!");
        return UFS_FAIL;
    }

    udb();
   
    // alternative
    if(emmc_write_sd_card(alternative_entry_lba, 1, (uint8_t*)(&block_of_entries)) == EMMC_FAIL){
        udbP("UFS GPT ERROR: Failed to write GPT partition entry!");
        return UFS_FAIL;
    }

    // calculate crc32 for entries
    uint8_t* buf = kmalloc(sizeof(GPT_PARTITION_ENTRY) * 128);

    udb();
   
    // read
    if(emmc_read_sd_card(gpt_header.partition_entry_lba, 32, buf) == EMMC_FAIL){
        udbP("UFS GPT ERROR: Failed to read GPT partition entries!");
        return UFS_FAIL;
    }

    delay_ms(1500);

    gpt_header.partition_entry_array_crc32 = crc32_aarch64(buf, sizeof(GPT_PARTITION_ENTRY) * 128); // write crc32

    kfree(buf);

    // calculate crc32 for default GPT header
    gpt_header.header_crc32 = 0ULL; // 0 first otherwise crc32 is going to be incorrect
    gpt_header.header_crc32 = crc32_aarch64((uint8_t*)&gpt_header, 92);

    udb();

    // write default GPT header
    if(emmc_write_sd_card(gpt_header.self_lba, 1, (uint8_t*)(&gpt_header)) == EMMC_FAIL){
        udbP("UFS GPT ERROR: Failed to write default GPT header!");
        return UFS_FAIL;
    }

    // prepare alternative header
    gpt_header.self_lba = gpt_header.alternate_lba;
    gpt_header.alternate_lba = 1;
    gpt_header.partition_entry_lba = alternative_entry_lba;

    // calculate crc32 for alternative GPT header
    gpt_header.header_crc32 = 0ULL; // 0 first otherwise crc32 is going to be incorrect
    gpt_header.header_crc32 = crc32_aarch64((uint8_t*)&gpt_header, 92);

    // write alternative GPT header
    if(emmc_write_sd_card(gpt_header.self_lba, 1, (uint8_t*)(&gpt_header)) == EMMC_FAIL){
        udbP("UFS GPT ERROR: Failed to write alternative GPT header!");
        return UFS_FAIL;
    }


    return UFS_SUCCESS;
}