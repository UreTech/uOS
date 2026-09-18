#include <u_kernel/filesystem/fat/fat.h>
#include <u_kernel/memory/u_memory.h>
#include <u_kernel/util/random/u_rand.h>
#include <u_kernel/util/util.h>
#include <u_kernel/filesystem/vfs/vfs.h>

// os driver instance handling
typedef struct fat32_instance
{
    uobject_ref storage_device;
    partition_info partition;
    fat32_bpb bpb;
    fat32_FSInfo FSInfo;

    uint64_t fat1_first_lba;
    uint64_t fat2_first_lba;
    uint32_t data_first_lba;

    struct fat32_instance* next;
} fat32_instance;

fat32_instance* instances = nullptr;

void fat32_create_new_instance(fat32_instance* instance_info){
    fat32_instance* current = instances;

    if(current != nullptr){
        // find end of chain
        while(current->next != nullptr){
            current = current->next;
        }

        current->next = (fat32_instance*)kmalloc(sizeof(fat32_instance));
        current = current->next;
        memcpy(current, instance_info, sizeof(fat32_instance));
        current->next = nullptr;
        return;

    }else{
        current = (fat32_instance*)kmalloc(sizeof(fat32_instance));
        memcpy(current, instance_info, sizeof(fat32_instance));
        current->next = nullptr;
        return;
    }
}

// driver

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
    else if(read_uint16_alignment_safe(&(bpb->bytes_per_sector)) != 512ULL){
        check = FAIL;
    }
    else if(read_uint16_alignment_safe(&(bpb->rootEntCount)) != 0){
        check = FAIL;
    }
    else if(read_uint16_alignment_safe(&(bpb->totSec16)) != 0){
        check = FAIL;
    }
    else if(read_uint16_alignment_safe(&(bpb->BPB_FATSz16)) != 0){
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

#define FAT32_ATTR_READ_ONLY_BIT ONEBIT(0)
#define FAT32_ATTR_HIDDEN_BIT ONEBIT(1)
#define FAT32_ATTR_SYSTEM_BIT ONEBIT(2)
#define FAT32_ATTR_VOLUME_ID_BIT ONEBIT(3)
#define FAT32_ATTR_DIRECTORY_BIT ONEBIT(4)
#define FAT32_ATTR_ARCHIVE_BIT ONEBIT(5)
#define FAT32_ATTR_LONG_FILE_NAME (0x0F)

typedef struct
{
    char DIR_Name[11]; // SFN
    uint8_t DIR_Attr;
    uint8_t DIR_NTRes; // not used (0)

    // time is not present so set to 0
    uint8_t DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI; // used in fat32
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;

    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} __attribute__((packed)) fat32_dir_entry;
_Static_assert(sizeof(fat32_dir_entry) == 32, "fat32_dir_entry must be 32 bytes");


uos_result fat32_create_file(u_fs_interface* interface, const char* parent, const char* file_name){


    return SUCCESS;
}

uos_result fat32_create_dir(u_fs_interface* interface, const char* parent, const char* dir_name){

    
    return SUCCESS;
}

uos_result mount_fat32_partition(partition_info partition, uobject_ref storage_device){
    udevice_emmc_storage_function_pointers* emmc =  _open_emmc_storage_device_(storage_device);

    if(emmc == nullptr){
        return FAIL;
    }

    fat32_instance inst;

    inst.storage_device = storage_device;
    inst.partition = partition;

    if(emmc->common.read(partition.start_lba + 0, 1, (uint8_t*)(&inst.bpb)) == FAIL){
        udbP("FAT ERROR: Failed to read FAT header!");
        if(uobject_close_object(storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    if(emmc->common.read(partition.start_lba + inst.bpb.BPB_FSInfo, 1, (uint8_t*)(&inst.FSInfo)) == FAIL){
        udbP("FAT ERROR: Failed to read FSInfo header!");
        if(uobject_close_object(storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    inst.fat1_first_lba = partition.start_lba + inst.bpb.reserved_sector_count;
    inst.fat2_first_lba = inst.fat1_first_lba + inst.bpb.BPB_FATSz32;
    inst.data_first_lba = inst.fat2_first_lba + inst.bpb.BPB_FATSz32;

    fat32_create_new_instance(&inst);

    if(uobject_close_object(storage_device) == FAIL){
        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        return FAIL;
    }

    u_fs_interface fat32_fsi;
    memcpy(fat32_fsi.fs_type_name, "FAT32", 6);
    fat32_fsi.storage_device = storage_device;
    memcpy(&fat32_fsi.partition, &partition, sizeof(partition_info));

    // set functions
    fat32_fsi.create = fat32_create_file;

    uobject_ref obj = uobject_create_fsi(partition.name, 0, fat32_fsi);
    if(obj == UOBJECT_NOT_DEFINED){
        udbP("FAT UOBJECT ERROR: Failed to create filesystem interface UOBJECT!");
        return FAIL;
    }

    return vfs_create_object_file("/mounts/", partition.name, obj, VFS_TYPE_FS_PARTITION, 0);
    return SUCCESS;
}
