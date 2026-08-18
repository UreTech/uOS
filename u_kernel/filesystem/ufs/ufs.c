#include <u_kernel/filesystem/ufs/ufs.h>
#include <u_kernel/util/u_ctypes.h>
#include <u_kernel//memory/u_memory.h>
#include <u_kernel/drivers/emmc/u_emmc.h>
#include <u_kernel/util/util.h>
#include <u_kernel/util/random/u_rand.h>
#include <u_kernel/util/u_cstr_util.h>
#include <u_kernel/timer/u_timer.h>
#include <u_kernel/objects/uobject.h>

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

void utf16_to_char_buf(uint16_t* _wchar, char* _char){
    size_t len = wstrlen(_wchar, 10000) + 1; // include null terminator
    for(size_t i = 0; i < len; i++){
        _char[i] = (unsigned char)_wchar[i];
    }
}

udevice_emmc_storage_function_pointers* _open_emmc_storage_device_(uobject_ref device_ref){
    uobject* storage_device_object = uobject_open_object(device_ref, UOBJECT_TYPE_DEVICE);

    if(storage_device_object == nullptr){
        udbP("UFS STORAGE DEVICE ERROR: Failed to open storage device!");
        return nullptr;
    }

    udevice* storage_device = (udevice*)(storage_device_object->obj_data);

    if(storage_device->type != UDEVICE_TYPE_EMMC_STORAGE){
        udbP("UFS STORAGE DEVICE ERROR: Storage device is not a emmc storage device!");
        if(uobject_close_object(device_ref) == FAIL){
            udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
            return nullptr;
        }
        return nullptr; 
    }

    return (udevice_emmc_storage_function_pointers*)(storage_device->symbols);
}

uos_result format_sd_gpt_with_pre_partitions(uobject_ref device_ref){

    udevice_emmc_storage_function_pointers* emmc = _open_emmc_storage_device_(device_ref);

    if(emmc == nullptr){
        return FAIL;
    }

    uint64_t total_lba_count = emmc->common.get_lba_count();
    uart_print("total size: ");
    uart_print_dec(emmc->get_card_information().csd.device_size);
    uart_print("\nblock size: ");
    uart_print_dec(emmc->get_card_information().csd.max_read_data_block_length);
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
    if(emmc->common.write(0, 1, (uint8_t*)(&protective_mbr)) == FAIL){
        udbP("UFS GPT ERROR: Failed to write protective mbr header!");
        if(uobject_close_object(device_ref) == FAIL){
            udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
            return UFS_FAIL;
        }
        return UFS_FAIL;
    }

    // prepare gpt header
    GPT_HEADER gpt_header = {0};

    gpt_header.signature = 0x5452415020494645; // "EFI PART"
    gpt_header.revision = 0x10000;
    gpt_header.header_size = 92ULL;
    gpt_header.alternate_lba = total_lba_count - 1; // last lba
    gpt_header.first_usable_lba = 34ULL; // (1lb offset) 1 gpt header + (128 / 4) 32 entry = 34th lba
    gpt_header.last_usable_lba = total_lba_count - 34; // 34rd lba from end

    gpt_header.disk_guid[0] = hardware_rng64();

    gpt_header.disk_guid[1] = hardware_rng64();


    gpt_header.self_lba = 1ULL; // this lba

    gpt_header.partition_entry_lba = 2ULL;
    gpt_header.number_of_partition_entries = 128;
    gpt_header.size_of_partition_entry = 128;

    gpt_header.partition_entry_array_crc32 = 0ULL; // set later
    gpt_header.header_crc32 = 0ULL; // set later

    // save for future
    uint64_t alternative_header_lba = gpt_header.alternate_lba;
    uint64_t alternative_entry_lba = gpt_header.alternate_lba - 32;

    GPT_PARTITION_ENTRY block_of_entries[4]; // 1 lba
    memset(block_of_entries, 0x0, sizeof(GPT_PARTITION_ENTRY) * 4); // zero

    // write empty 31 entry
    for(uint32_t i = 1; i < 32; i++){
        // default
        if(emmc->common.write(gpt_header.partition_entry_lba + i, 1, (uint8_t*)(&block_of_entries)) == FAIL){
            udbP("UFS GPT ERROR: Failed to write GPT partition entry!");
            if(uobject_close_object(device_ref) == FAIL){
                udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
                return UFS_FAIL;
            }
            return UFS_FAIL;
        }

        // alternative
        if(emmc->common.write(alternative_entry_lba + i, 1, (uint8_t*)(&block_of_entries)) == FAIL){
            udbP("UFS GPT ERROR: Failed to write GPT partition entry!");
            if(uobject_close_object(device_ref) == FAIL){
                udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
                return UFS_FAIL;
            }
            return UFS_FAIL;
        }
    }

    // prepare preset partitions

    uint64_t first_partitionable_lba = 34;
    size_t not_allocated_lba_count = total_lba_count - 66; // 33 default + 33 alternative

    // FAT for bootloader
    char_to_utf16_buf("uBOOT", block_of_entries[0].patrtition_name);
    block_of_entries[0].partition_type_guid[0] = 0x11D2F81FC12A7328ULL;
    block_of_entries[0].partition_type_guid[1] = 0x3BC93EC9A0004BBAULL;
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

    // write new entries
    // default
    if(emmc->common.write(gpt_header.partition_entry_lba, 1, (uint8_t*)(&block_of_entries)) == FAIL){
        udbP("UFS GPT ERROR: Failed to write GPT partition entry!");
        if(uobject_close_object(device_ref) == FAIL){
            udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
            return UFS_FAIL;
        }
        return UFS_FAIL;
    }
   
    // alternative
    if(emmc->common.write(alternative_entry_lba, 1, (uint8_t*)(&block_of_entries)) == FAIL){
        udbP("UFS GPT ERROR: Failed to write GPT partition entry!");
        if(uobject_close_object(device_ref) == FAIL){
            udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
            return UFS_FAIL;
        }
        return UFS_FAIL;
    }

    // calculate crc32 for entries
    uint8_t* buf = kmalloc(sizeof(GPT_PARTITION_ENTRY) * 128);
   
    // read
    if(emmc->common.read(gpt_header.partition_entry_lba, 32, buf) == FAIL){
        udbP("UFS GPT ERROR: Failed to read GPT partition entries!");
        if(uobject_close_object(device_ref) == FAIL){
            udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
            return UFS_FAIL;
        }
        return UFS_FAIL;
    }

    delay_ms(1500);

    gpt_header.partition_entry_array_crc32 = crc32_aarch64(buf, sizeof(GPT_PARTITION_ENTRY) * 128); // write crc32

    kfree(buf);

    // calculate crc32 for default GPT header
    gpt_header.header_crc32 = 0ULL; // 0 first otherwise crc32 is going to be incorrect
    gpt_header.header_crc32 = crc32_aarch64((uint8_t*)&gpt_header, 92);

    // write default GPT header
    if(emmc->common.write(gpt_header.self_lba, 1, (uint8_t*)(&gpt_header)) == FAIL){
        udbP("UFS GPT ERROR: Failed to write default GPT header!");
        if(uobject_close_object(device_ref) == FAIL){
            udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
            return UFS_FAIL;
        }
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
    if(emmc->common.write(gpt_header.self_lba, 1, (uint8_t*)(&gpt_header)) == FAIL){
        udbP("UFS GPT ERROR: Failed to write alternative GPT header!");
        if(uobject_close_object(device_ref) == FAIL){
            udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
            return UFS_FAIL;
        }
        return UFS_FAIL;
    }

    if(uobject_close_object(device_ref) == FAIL){
        udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
        return UFS_FAIL;
    }

    return UFS_SUCCESS;
}

// returns partial success if header is damaged
// NOTE: this function does not check partition bounds
uos_result _check_gpt_header_(GPT_HEADER head){

    // check basic stuff
    if(head.signature != 0x5452415020494645){
        return FAIL;
    }else if(head.revision != 0x10000){
        return FAIL;
    }else if((head.header_size < 92ULL) || (head.header_size > sizeof(GPT_HEADER))){
        return FAIL;
    }
    // pass

    // check crc32
    uint32_t head_crc32 = head.header_crc32;
    
    head.header_crc32 = 0ULL;
    if(head_crc32 != crc32_aarch64((uint8_t*)&head, head.header_size)){
        return PARTIAL_SUCCESS
    }else{
        return SUCCESS;
    }
}

uos_result get_partition_from_device(uobject_ref device_ref, partition_info* partitions, size_t max_entry_count_to_read){

    partition_info result;
    result.fs_guid_type = FS_GUID_INVALID;

    // open device
    udevice_emmc_storage_function_pointers* emmc = _open_emmc_storage_device_(device_ref);

    if(emmc == nullptr){
        return FAIL;
    }

    size_t total_lba_count = emmc->common.get_lba_count();
    size_t lba_size = emmc->get_card_information().csd.max_read_data_block_length;

    // check GPT header integrity

    // read main head
    GPT_HEADER* main_head = palloc(1);
    GPT_HEADER* alt_head = palloc(1);

    if(emmc->common.read(1, 1, main_head) == FAIL){
        udbP("UFS GPT ERROR: Failed to read main head!");
        pfree(main_head, 1);
        pfree(alt_head, 1);
        if(uobject_close_object(device_ref) == FAIL){
            udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    // try to read alternative head (last lba)
    if(emmc->common.read(total_lba_count - 1, 1, alt_head) == FAIL){
        udbP("UFS GPT ERROR: Failed to read alternative head!");
        pfree(main_head, 1);
        pfree(alt_head, 1);
        if(uobject_close_object(device_ref) == FAIL){
            udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    if(_check_gpt_header_(*main_head) != SUCCESS){
            udbP("UFS GPT WARN: Main head is corrupted or disk does not have GPT partition!");

            if(_check_gpt_header_(*alt_head) != SUCCESS){
                // farrrt
                udbP("UFS GPT WARN: Alternative head is corrupted or disk does not have GPT partition!");
                udbP("UFS GPT ERROR: No any healty GPT header found.");
                pfree(main_head, 1);
                pfree(alt_head, 1);
                return FAIL;
            }else{
                // repair main header with alternative
                *main_head = *alt_head; // copy
                main_head->alternate_lba = total_lba_count - 1;
                main_head->header_crc32 = 0ULL; // set later
                main_head->partition_entry_lba = 2ULL;
                main_head->self_lba = 1ULL;

                main_head->header_crc32 = crc32_aarch64((uint8_t*)main_head, main_head->header_size);

                // write
                if(emmc->common.write(main_head->self_lba, 1, main_head) == FAIL){
                    udbP("UFS GPT ERROR: Failed to write repaired main head!");
                    pfree(main_head, 1);
                    pfree(alt_head, 1);
                    if(uobject_close_object(device_ref) == FAIL){
                        udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
                    }
                    return FAIL;
                }
            }
    }else{
         if(_check_gpt_header_(*alt_head) != SUCCESS){
                // repair if alt head is corrupted
                udbP("UFS GPT WARN: Alternative head is corrupted! Reparing alternative head.");

                *alt_head = *main_head; // copy
                alt_head->alternate_lba = 1;
                alt_head->header_crc32 = 0ULL; // set later
                alt_head->partition_entry_lba = total_lba_count - 1 - (((alt_head->size_of_partition_entry * alt_head->number_of_partition_entries) + 511)/ lba_size);
                alt_head->self_lba = total_lba_count - 1;

                alt_head->header_crc32 = crc32_aarch64((uint8_t*)alt_head, alt_head->header_size);

                // write
                if(emmc->common.write(alt_head->self_lba, 1, alt_head) == FAIL){
                    udbP("UFS GPT ERROR: Failed to write repaired alternative head!");
                    pfree(main_head, 1);
                    pfree(alt_head, 1);
                    if(uobject_close_object(device_ref) == FAIL){
                        udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
                    }
                    return FAIL;
                }
            }
    }

    // check GPT entry array integrity

    // read main entry array
    size_t total_array_size =  (main_head->size_of_partition_entry * main_head->number_of_partition_entries);
    GPT_PARTITION_ENTRY* entry_array = kmalloc(total_array_size); // main
    GPT_PARTITION_ENTRY* alt_entry_array = kmalloc(total_array_size);

    if(emmc->common.read(main_head->partition_entry_lba, ((total_array_size + 511) / lba_size), entry_array) == FAIL){
        udbP("UFS GPT ERROR: Failed to read main entry array!");
        pfree(main_head, 1);
        pfree(alt_head, 1);
        kfree(entry_array);
        kfree(alt_entry_array);
        if(uobject_close_object(device_ref) == FAIL){
            udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    if(emmc->common.read(alt_head->partition_entry_lba, ((total_array_size + 511) / lba_size), alt_entry_array) == FAIL){
        udbP("UFS GPT ERROR: Failed to read alternative entry array!");
        pfree(main_head, 1);
        pfree(alt_head, 1);
        kfree(entry_array);
        kfree(alt_entry_array);
        if(uobject_close_object(device_ref) == FAIL){
            udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    // check crc32 for main array
    if(crc32_aarch64((uint8_t*)entry_array, (main_head->size_of_partition_entry * main_head->number_of_partition_entries)) != main_head->partition_entry_array_crc32){
            udbP("UFS GPT WARN: Main entry array is corrupted! Repairing.");

            // try to repair main entry array with alternative entry array
            
            // check crc32 for alternative array
            if(crc32_aarch64((uint8_t*)alt_entry_array, (alt_head->size_of_partition_entry * alt_head->number_of_partition_entries)) != alt_head->partition_entry_array_crc32){
                udbP("UFS GPT WARN: Alternative entry array is corrupted!");
                udbP("UFS GPT ERROR: Both entry arrays are corrupted! No any easy repair possiable.");
                pfree(main_head, 1);
                pfree(alt_head, 1);
                kfree(entry_array);
                kfree(alt_entry_array);
                if(uobject_close_object(device_ref) == FAIL){
                    udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                return FAIL;
            }

            // repair main
            // write to main
            memcpy(entry_array, alt_entry_array, total_array_size);
            if(emmc->common.write(main_head->partition_entry_lba, ((total_array_size + 511) / lba_size), entry_array) == FAIL){
                udbP("UFS GPT ERROR: Failed to write repair to main entry array!");
                pfree(main_head, 1);
                pfree(alt_head, 1);
                kfree(entry_array);
                kfree(alt_entry_array);
                if(uobject_close_object(device_ref) == FAIL){
                    udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                return FAIL;
            }
    }else{            
            // check crc32 for alternative array
            if(crc32_aarch64((uint8_t*)alt_entry_array, (main_head->size_of_partition_entry * main_head->number_of_partition_entries)) != alt_head->partition_entry_array_crc32){
                udbP("UFS GPT WARN: Alternative entry array is corrupted! Repairing.");

                // try to repair main entry array with alternative entry array
                
                // repair alternative
                // write to alternative
                memcpy(alt_entry_array, entry_array, total_array_size);
                if(emmc->common.write(alt_head->partition_entry_lba, ((total_array_size + 511) / lba_size), alt_entry_array) == FAIL){
                    udbP("UFS GPT ERROR: Failed to write repair to main entry array!");
                    pfree(main_head, 1);
                    pfree(alt_head, 1);
                    kfree(entry_array);
                    kfree(alt_entry_array);
                    if(uobject_close_object(device_ref) == FAIL){
                        udbP("UFS STORAGE DEVICE ERROR: Failed to close storage device!");
                    }
                    return FAIL;
                }
            }
    }

    size_t last_partition_entry_index = 0;
    for (size_t i = 0; i < main_head->number_of_partition_entries; i++)
    {
        if(last_partition_entry_index >= max_entry_count_to_read){
            break;
        }

        if(!(entry_array[i].partition_type_guid[0] == 0 && entry_array[i].partition_type_guid[1] == 0)){
            partitions[last_partition_entry_index].start_lba = entry_array[i].starting_lba;
            partitions[last_partition_entry_index].lba_count = entry_array[i].ending_lba - entry_array[i].starting_lba + 1;
            utf16_to_char_buf(entry_array[i].patrtition_name, partitions[last_partition_entry_index].name);
            last_partition_entry_index++;
        }else{
            // empty skip...
        }
    }
    
    pfree(main_head, 1);
    pfree(alt_head, 1);
    kfree(entry_array);
    kfree(alt_entry_array);

    return SUCCESS;
}
