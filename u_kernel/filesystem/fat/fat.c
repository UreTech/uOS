#include <u_kernel/filesystem/fat/fat.h>
#include <u_kernel/memory/u_memory.h>

uos_result check_fat32_partition(partition_info partition, uobject_ref storage_device){
    udevice_emmc_storage_function_pointers* emmc =  _open_emmc_storage_device_(storage_device);

    if(emmc == nullptr) return FAIL;

    // read BPB
    fat32_bpb* bpb = kmalloc(sizeof(fat32_bpb));

    if(emmc->common.read(partition.start_lba + 0, 1, (uint8_t*)(bpb)) == FAIL){
        udbP("FAT GPT ERROR: Failed to read FAT header!");
        kfree(bpb);
        if(uobject_close_object(storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return UFS_FAIL;
    }

    // check head
    uos_result check = SUCCESS;
    uint8_t spc = bpb->sector_per_cluster;
    if(bpb->BS_Sign != 0xAA55){
        check = FAIL;
    }
    else if(bpb->BPB_FSVer != 0ULL){
        check = FAIL;
    }
    else if(bpb->bytes_per_sector != 512ULL){
        check = FAIL;
    }
    else if(bpb->rootEntCount != 0){
        check = FAIL;
    }
    else if(bpb->totSec16 != 0){
        check = FAIL;
    }
    else if(bpb->BPB_FATSz16 != 0){
        check = FAIL;
    }
    else if(bpb->BPB_FATSz32 == 0){
        check = FAIL;
    }
    else if(bpb->number_of_fats == 0){
        check = FAIL;
    }
    else if(bpb->sector_per_cluster > 128ULL){
        check = FAIL;
    }
    else if (spc == 0 || (spc & (spc - 1)) != 0){
        check = FAIL;
    }
    else if (bpb->reserved_sector_count == 0) {
        check = FAIL;
    }
    else if (bpb->TotSec32 == 0) {
        check = FAIL;
    }
    else if (bpb->BPB_RootClus < 2) {
        check = FAIL;
    }
    
    kfree(bpb);
    if(uobject_close_object(storage_device) == FAIL){
        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        return UFS_FAIL;
    }
    return check;
}

uint64_t fatgen103(size_t partition_lba_count, uint16_t reserved_sector_count, uint8_t sector_per_cluster, uint8_t num_of_fats){
    
}