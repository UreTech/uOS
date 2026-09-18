#include <u_kernel/filesystem/fat/fat.h>
#include <u_kernel/memory/u_memory.h>
#include <u_kernel/util/random/u_rand.h>
#include <u_kernel/util/util.h>

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

uos_result create_fat32_partition(partition_info partition, uobject_ref storage_device){
    udevice_emmc_storage_function_pointers* emmc =  _open_emmc_storage_device_(storage_device);

    if(emmc == nullptr) return FAIL;

    // create BPB
    fat32_bpb bpb;
    memset(&bpb, 0x0, sizeof(fat32_bpb));
    bpb.jmpBoot[0] = 0xEB;
    bpb.jmpBoot[1] = 0x58;
    bpb.jmpBoot[2] = 0x90;
    bpb.BPB_SecPerTrk = 63;
    bpb.BPB_NumHeads = 255;
    bpb.BPB_HiddSec = partition.start_lba;  
    memcpy(bpb.OEMName, "MSDOS5.0", 8);
    write_uint16_alignment_safe(&bpb.bytes_per_sector, emmc->get_card_information().csd.max_read_data_block_length);
    uint16_t safe_holded_bytes_per_sector = emmc->get_card_information().csd.max_read_data_block_length;
    bpb.sector_per_cluster = 8; // 4kb
    bpb.reserved_sector_count = 32;
    bpb.number_of_fats = 2;
    bpb.BPB_Media = 0xF8;
    bpb.TotSec32 = partition.lba_count;
    uint64_t div = (bpb.sector_per_cluster * (safe_holded_bytes_per_sector / 4)) + bpb.number_of_fats; 
    bpb.BPB_FATSz32 = (partition.lba_count - (bpb.reserved_sector_count) + div - 1) / div; // (total - reserved) / ((sector_per_cluster * (safe_holded_bytes_per_sector / 8)) + 2) // +2 for 2 fat tables
    
    // fix 2 dummy cluster entries
    while(true){
        uint64_t data_sector_count = bpb.TotSec32 - bpb.reserved_sector_count - (bpb.BPB_FATSz32 * bpb.number_of_fats);
        uint64_t cluster_count = data_sector_count / bpb.sector_per_cluster;
        uint64_t fat_mapped_cluster_count = (bpb.BPB_FATSz32 * safe_holded_bytes_per_sector) / 4;
        if(fat_mapped_cluster_count < cluster_count + 2){
            bpb.BPB_FATSz32++;
        }else{
            break;
        }
    }

    bpb.BPB_ExtFlags = 0x0000;
    bpb.BPB_FSVer = 0x0000;
    bpb.BPB_RootClus = 2; // 0 & 1 is not healty to use
    bpb.BPB_FSInfo = 1; // 2nd sector
    bpb.BPB_BkBootSec = 0; // self
    bpb.BS_DriveNum = 0x80;
    bpb.BS_BootSig = 0x29;
    write_uint32_alignment_safe(&bpb.BS_VolID, hardware_rng32()); // we dont have rtc yet so just pass some random number
    memcpy(bpb.BS_VolLab, "uOS Volume", 11);
    memcpy(bpb.BS_FilSysType, "FAT32   ", 8);
    bpb.BS_Sign = 0xAA55;

    // write bpb to first lba in partition
    if(emmc->common.write(partition.start_lba, 1, (uint8_t*)&bpb) == FAIL){
        udbP("FAT GPT ERROR: Failed to write BPB header!");
        if(uobject_close_object(storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    // create FSInfo
    fat32_FSInfo fsi;
    memset(&fsi, 0x0, sizeof(fat32_FSInfo));
    fsi.FSI_LeadSig = 0x41615252;
    fsi.FSI_StrucSig = 0x61417272;
    fsi.FSI_Free_Count = FAT32_FSINFO_UNKNOWN;
    fsi.FSI_Nxt_Free = FAT32_FSINFO_UNKNOWN;
    fsi.FSI_TrailSig = 0xAA550000;

    // write FSInfo to first lba in partition
    if(emmc->common.write(partition.start_lba + bpb.BPB_FSInfo, 1, (uint8_t*)&fsi) == FAIL){
        udbP("FAT GPT ERROR: Failed to write FSInfo header!");
        if(uobject_close_object(storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    // create & write fat tables
    uint64_t fat1_lba = partition.start_lba + bpb.reserved_sector_count;
    uint64_t fat2_lba = fat1_lba + bpb.BPB_FATSz32;
    uint64_t fat_entries_per_page = safe_holded_bytes_per_sector / 4;

    uint32_t* fat_page = kmalloc(safe_holded_bytes_per_sector);
    memset(fat_page, 0x0, safe_holded_bytes_per_sector);

    // write empty clusters first
    for(size_t i = 1; i < bpb.BPB_FATSz32; i++){
        // fat1
        if(emmc->common.write(fat1_lba + i, 1, (uint8_t*)fat_page) == FAIL){
            udbP("FAT GPT ERROR: Failed to write FAT1!");
            kfree(fat_page);
            if(uobject_close_object(storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            return FAIL;
        }

        // fat2
        if(emmc->common.write(fat2_lba + i, 1, (uint8_t*)fat_page) == FAIL){
            udbP("FAT GPT ERROR: Failed to write FAT2!");
            kfree(fat_page);
            if(uobject_close_object(storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            return FAIL;
        }
    }

    // then minimum required clusters
    fat_page[0] = 0x0FFFFFF8;
    fat_page[1] = 0xFFFFFFFF;
    fat_page[2] = 0x0FFFFFFF; // root cluster

    // fat1
    if(emmc->common.write(fat1_lba + 0, 1, (uint8_t*)fat_page) == FAIL){
        udbP("FAT GPT ERROR: Failed to write FAT1!");
        kfree(fat_page);
        if(uobject_close_object(storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    // fat2
    if(emmc->common.write(fat2_lba + 0, 1, (uint8_t*)fat_page) == FAIL){
        udbP("FAT GPT ERROR: Failed to write FAT2!");
        kfree(fat_page);
        if(uobject_close_object(storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }
    kfree(fat_page);

    // fill root cluster with zeros
    uint32_t* zeroed_root_clus = kmalloc(safe_holded_bytes_per_sector * bpb.sector_per_cluster);
    memset(zeroed_root_clus, 0x0, safe_holded_bytes_per_sector * bpb.sector_per_cluster);

    uint64_t first_cluster_lba = partition.start_lba + bpb.reserved_sector_count + (bpb.BPB_FATSz32 * bpb.number_of_fats);

    if(emmc->common.write(first_cluster_lba, bpb.sector_per_cluster, (uint8_t*)zeroed_root_clus) == FAIL){
        udbP("FAT GPT ERROR: Failed to write RootCluster!");
        kfree(zeroed_root_clus);
        if(uobject_close_object(storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }
    kfree(zeroed_root_clus);

    if(uobject_close_object(storage_device) == FAIL){
        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
    }
    return SUCCESS;
}

uos_result mount_fat32_partition(partition_info partition, uobject_ref storage_device){
    
}
