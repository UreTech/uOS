#include <u_kernel/filesystem/fat/fat.h>
#include <u_kernel/memory/u_memory.h>
#include <u_kernel/util/random/u_rand.h>
#include <u_kernel/util/util.h>
#include <u_kernel/filesystem/vfs/vfs.h>
#include <u_kernel/objects/fsi/fsi.h>
#include <u_kernel/util/u_cstr_util.h>


// bios parameter block
typedef struct
{
    // common field
    uint8_t jmpBoot[3]; // not used
    char OEMName[8]; // "MSDOS5.0"
    uint16_t bytes_per_sector; // disk LBA size (512 for sd card)
    uint8_t sector_per_cluster; // it name explains it self
    uint16_t reserved_sector_count; // generaly 32
    uint8_t number_of_fats; // generaly 2
    uint16_t rootEntCount; // not used in fat32
    uint16_t totSec16; // not used in fat32
    uint8_t BPB_Media; // 0xF8
    uint16_t BPB_FATSz16; // not used in fat32
    uint16_t BPB_SecPerTrk; // not used in fat32
    uint16_t BPB_NumHeads; // not used in fat32
    uint32_t BPB_HiddSec; // not used in fat32
    uint32_t TotSec32; // total sector (512byte LBA) count of FAT32 partition
    // FAT32 field
    uint32_t BPB_FATSz32; // size of fat in sectors
    uint16_t BPB_ExtFlags; // flags
    uint16_t BPB_FSVer; // 0
    uint32_t BPB_RootClus; // first cluster number
    uint16_t BPB_FSInfo; // sector of FSInfo structure in offset from top of FAT32 volume (usualy set to 1)
    uint16_t BPB_BkBootSec; // sector of backup boot sector in offset from top of FAT32 volume (usualy set to 6)
    uint8_t BPB_Reserved[12]; // not used
    uint8_t BS_DriveNum; // 0x80
    uint8_t BS_Reserved; // 0
    uint8_t BS_BootSig; // 0x29
    uint32_t BS_VolID; // a random serial number
    char BS_VolLab[11]; // volume label
    char BS_FilSysType[8]; // always "FAT32   "
    uint8_t BS_BootCode32[420]; // not used
    uint16_t BS_Sign; // always 0xAA55
} __attribute__((packed)) fat32_bpb;
_Static_assert(sizeof(fat32_bpb) == 512, "fat32_bpb must be 512 bytes");

//FSInfo struct
typedef struct
{
    uint32_t FSI_LeadSig; // 0x41615252
    uint8_t FSI_Reserved1[480];
    uint32_t FSI_StrucSig; // 0x61417272
    uint32_t FSI_Free_Count; // last known free cluster count (0xFFFFFFFF = unknown)
    uint32_t FSI_Nxt_Free; // last known next free cluster (0xFFFFFFFF = unknown)
    uint8_t FSI_Reserved2[12];
    uint32_t FSI_TrailSig; // 0xAA550000
} __attribute__((packed)) fat32_FSInfo;
_Static_assert(sizeof(fat32_FSInfo) == 512, "fat32_FSInfo must be 512 bytes");
#define FAT32_FSINFO_UNKNOWN (0xFFFFFFFF)

// os driver instance handling
typedef struct fat32_instance
{
    uobject_ref storage_device;
    u_mutex instance_lock;
    partition_info partition;
    fat32_bpb bpb;
    fat32_FSInfo FSInfo;

    uint64_t fat1_first_lba;
    uint64_t fat2_first_lba;
    uint32_t data_first_lba;
    uint32_t data_cluster_count;

    struct fat32_instance* next;
} fat32_instance;

fat32_instance* instances = nullptr;

void fat32_create_new_instance(fat32_instance* instance_info){

    if (instances == nullptr) {
        instances = (fat32_instance*)kmalloc(sizeof(fat32_instance));
        memcpy(instances, instance_info, sizeof(fat32_instance));
        instances->next = nullptr;
        return;
    }

    fat32_instance* current = instances;

    u_mutex_init(&(current->instance_lock));

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

// returns nullptr if not found
fat32_instance* fat32_find_instance(partition_info partition){
    fat32_instance* current = instances;
    while(current != nullptr){
        if(!memcmp(&current->partition, &partition, sizeof(partition_info))){
            return current;
        }
        current = current->next;
    }
    return nullptr; // not found
}

// driver
uos_result check_fat32_partition(partition_info partition, uobject_ref storage_device){
    udevice_storage_function_pointers* stor_device = _open_storage_device_(storage_device);

    if(stor_device == nullptr) return FAIL;

    // read BPB
    fat32_bpb* bpb = kmalloc(sizeof(fat32_bpb));

    if(stor_device->read(partition.start_lba + 0, 1, (uint8_t*)(bpb)) == FAIL){
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
    udevice_storage_function_pointers* stor_device = _open_storage_device_(storage_device);

    if(stor_device == nullptr) return FAIL;

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
    write_uint16_alignment_safe(&bpb.bytes_per_sector, stor_device->get_lba_size());
    uint16_t safe_holded_bytes_per_sector = stor_device->get_lba_size();
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
    if(stor_device->write(partition.start_lba, 1, (uint8_t*)&bpb) == FAIL){
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

    // write FSInfo
    if(stor_device->write(partition.start_lba + bpb.BPB_FSInfo, 1, (uint8_t*)&fsi) == FAIL){
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
        if(stor_device->write(fat1_lba + i, 1, (uint8_t*)fat_page) == FAIL){
            udbP("FAT GPT ERROR: Failed to write FAT1!");
            kfree(fat_page);
            if(uobject_close_object(storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            return FAIL;
        }

        // fat2
        if(stor_device->write(fat2_lba + i, 1, (uint8_t*)fat_page) == FAIL){
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
    if(stor_device->write(fat1_lba + 0, 1, (uint8_t*)fat_page) == FAIL){
        udbP("FAT GPT ERROR: Failed to write FAT1!");
        kfree(fat_page);
        if(uobject_close_object(storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    // fat2
    if(stor_device->write(fat2_lba + 0, 1, (uint8_t*)fat_page) == FAIL){
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

    if(stor_device->write(first_cluster_lba, bpb.sector_per_cluster, (uint8_t*)zeroed_root_clus) == FAIL){
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

#define FAT32_CLUSTER_FREE (0x00000000)
#define FAT32_NEW_CLUSTER FAT32_CLUSTER_FREE
#define FAT32_CLUSTER_RESERVED (0x00000001)

#define FAT32_CLUSTER_ALLOCATED_MIN (0x00000002)
#define FAT32_CLUSTER_ALLOCATED_MAX (0x0FFFFFF6)
#define FAT32_CHECK_IS_CLUSTER_ALLOCATED(cluster_value) ((cluster_value >= FAT32_CLUSTER_ALLOCATED_MIN) && (cluster_value <= FAT32_CLUSTER_ALLOCATED_MAX))

#define FAT32_CLUSTER_BAD_CLUSTER (0x0FFFFFF7)
#define FAT32_CLUSTER_CHAIN_END_MIN (0x0FFFFFF8)
#define FAT32_CLUSTER_CHAIN_END_MAX (0x0FFFFFFF)
#define FAT32_CHECK_IS_CLUSTER_END(cluster_value) ((cluster_value >= FAT32_CLUSTER_CHAIN_END_MIN) && (cluster_value <= FAT32_CLUSTER_CHAIN_END_MAX))

typedef uint32_t fat32_cluster_entry;

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
#define FAT32_DIR_ENTRY_FREE (0xE5)
#define FAT32_DIR_ENTRY_FREE_START (0x00)

typedef struct
{
    uint8_t LDIR_Ord; // id of lfn entry in chain
    wide_char_t LDIR_Name1[5];
    uint8_t LDIR_Attr;
    uint8_t LDIR_Type;
    uint8_t LDIR_Chksum;
    wide_char_t LDIR_Name2[6];
    uint16_t LDIR_FstClusLO; // always 0
    wide_char_t LDIR_Name3[2];
} __attribute__((packed)) fat32_long_dir_entry;
_Static_assert(sizeof(fat32_long_dir_entry) == 32, "fat32_long_dir_entry must be 32 bytes");
#define FAT32_LFN_ID_FLAG_BEGIN ONEBIT(6) // 6th bit

uint8_t fat32_calculate_lfn_chksum(char* sfn){
    uint8_t sum = 0;
    for(int i  = 0; i < 11; i++){
        sum = (sum >> 1) + (sum << 7) + sfn[i];
    }
    return sum;
}

// buffer should have 256 wchars
size_t prepend_lfn(fat32_long_dir_entry* long_dir, wide_char_t* buffer,  size_t current_read_len){
    wide_char_t* tmp = kmalloc((256 + 1 + 13) * sizeof(wide_char_t)); // +13 for shift area
    memcpy(tmp + 13, buffer, current_read_len * sizeof(wide_char_t)); // shift to right

    // copy to begining
    memcpy(tmp + 0, long_dir->LDIR_Name1, 5 * sizeof(wide_char_t));
    memcpy(tmp + 0 + 5, long_dir->LDIR_Name2, 6 * sizeof(wide_char_t));
    memcpy(tmp + 0 + 5 + 6, long_dir->LDIR_Name3, 2 * sizeof(wide_char_t));

    // copy back to original buffer
    memcpy(buffer, tmp, (current_read_len + 13) * sizeof(wide_char_t));
    kfree(tmp);
    return current_read_len + 13;
}  

// returns last offset
// NOTE: this function puts reversly so we dont need to do it reversing in use area
size_t put_in_lfn(fat32_long_dir_entry* long_dir, const wide_char_t* buffer, size_t current_wrote_len){
    size_t total_name_len = wstrlen(buffer, 256 + 1) + 1; // +1 nullterm
    if((total_name_len - current_wrote_len) < 13){
        // short entry
        size_t write_start = 0;
        size_t write_len = (total_name_len - current_wrote_len) % 13;
        size_t wrote_len = 0;
        if(write_len >= 5){
            memcpy(long_dir->LDIR_Name1, buffer + write_start + 0, 5 * sizeof(wide_char_t));
            write_len -= 5;
            write_start += 5;
            wrote_len += 5;

            if(write_len >= 6){
                memcpy(long_dir->LDIR_Name2, buffer + write_start + 0, 6 * sizeof(wide_char_t));
                write_len -= 6;
                write_start += 6;
                wrote_len += 6;

                if(write_len >= 2){
                    memcpy(long_dir->LDIR_Name3, buffer + write_start + 0, 2 * sizeof(wide_char_t));
                    write_len -= 2;
                    write_start += 2;
                    wrote_len += 2;

                    return current_wrote_len + wrote_len;
                    
                }else{
                    memcpy(long_dir->LDIR_Name3, buffer + write_start + 0, write_len * sizeof(wide_char_t));
                    wrote_len += write_len;
                    return current_wrote_len + wrote_len;
                }
                
            }else{
                memcpy(long_dir->LDIR_Name2, buffer + write_start + 0, write_len * sizeof(wide_char_t));
                wrote_len += write_len;
                return current_wrote_len + wrote_len;
            }

        }else{
            memcpy(long_dir->LDIR_Name1, buffer + write_start + 0, write_len * sizeof(wide_char_t));
            wrote_len += write_len;
            return current_wrote_len + wrote_len;
        }
        
    }else{
        // long entry
        if((total_name_len - current_wrote_len) % 13 == 0){
            // full/aligned
            size_t write_start = total_name_len - current_wrote_len - 13;
            memcpy(long_dir->LDIR_Name1, buffer + write_start + 0, 5 * sizeof(wide_char_t));
            memcpy(long_dir->LDIR_Name2, buffer + write_start + 0 + 5, 6 * sizeof(wide_char_t));
            memcpy(long_dir->LDIR_Name3, buffer + write_start + 0 + 5 + 6, 2 * sizeof(wide_char_t));
            return current_wrote_len + 13;
        }else{
            // half/unaligned
            size_t write_len = (total_name_len - current_wrote_len) % 13;
            size_t wrote_len = 0;
            size_t write_start = (total_name_len - current_wrote_len) - write_len;

            // fill with 0xFF
            memset(long_dir->LDIR_Name1, 0xFF, 5 * sizeof(wide_char_t));
            memset(long_dir->LDIR_Name2, 0xFF, 6 * sizeof(wide_char_t));
            memset(long_dir->LDIR_Name3, 0xFF, 2 * sizeof(wide_char_t));
            if(write_len >= 5){
                memcpy(long_dir->LDIR_Name1, buffer + write_start + 0, 5 * sizeof(wide_char_t));
                write_len -= 5;
                write_start += 5;
                wrote_len += 5;

                if(write_len >= 6){
                    memcpy(long_dir->LDIR_Name2, buffer + write_start + 0, 6 * sizeof(wide_char_t));
                    write_len -= 6;
                    write_start += 6;
                    wrote_len += 6;

                    if(write_len >= 2){
                        memcpy(long_dir->LDIR_Name3, buffer + write_start + 0, 2 * sizeof(wide_char_t));
                        write_len -= 2;
                        write_start += 2;
                        wrote_len += 2;

                        return current_wrote_len + wrote_len;
                        
                    }else{
                        memcpy(long_dir->LDIR_Name3, buffer + write_start + 0, write_len * sizeof(wide_char_t));
                        wrote_len += write_len;
                        return current_wrote_len + wrote_len;
                    }
                
                }else{
                    memcpy(long_dir->LDIR_Name2, buffer + write_start + 0, write_len * sizeof(wide_char_t));
                    wrote_len += write_len;
                    return current_wrote_len + wrote_len;
                }

            }else{
                memcpy(long_dir->LDIR_Name1, buffer + write_start + 0, write_len * sizeof(wide_char_t));
                wrote_len += write_len;
                return current_wrote_len + wrote_len;
            }
        }
    }
}

uos_result fat32_check_name(const wide_char_t* name)
{
    size_t len = wstrlen(name, 256);

    if (len == 0 || len > 255) return FAIL;

    for (size_t i = 0; i < len; i++)
    {
        wide_char_t c = name[i];

        if (c < 0x20)
            return FAIL;

        switch (c)
        {
            case '"':
            case '*':
            case '/':
            case ':':
            case '<':
            case '>':
            case '?':
            case '\\':
            case '|':
                return FAIL;
        }
    }

    return SUCCESS;
}

// NOTE: Buffer should be 11 bytes
void generate_random_short_name(char* buf){
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for(uint8_t i = 0; i < 11; i++){
        buf[i] = charset[hardware_rng32() % 36];
    }
}

// creates a virtual entry for root directory
void fat32_get_root_dir_virt_entry(fat32_instance* inst, fat32_dir_entry* result_entry){
    memcpy(result_entry->DIR_Name, "ROOTDIRECTR", 11);
    result_entry->DIR_Attr = FAT32_ATTR_DIRECTORY_BIT;
    result_entry->DIR_NTRes = 0x0;
    result_entry->DIR_FstClusLO = (inst->bpb.BPB_RootClus) & 0xFFFF;
    result_entry->DIR_FstClusHI = (inst->bpb.BPB_RootClus >> 16);
    result_entry->DIR_FileSize = 0;
}

typedef struct
{
    size_t chain_len;
    uint32_t* chain;
}fat32_cluster_chain;

// NOTE: DO NOT FORGET TO FREE chain IN STRUCT AFTER USE!
fat32_cluster_chain fat32_get_chain(fat32_instance* inst, udevice_storage_function_pointers* stor_device, uint32_t chain_start_cluster){
    fat32_cluster_chain result;
    result.chain = nullptr;
    result.chain_len = 0;

    if(!FAT32_CHECK_IS_CLUSTER_ALLOCATED(chain_start_cluster)){
        udbP("FAT ERROR: Start chain is not valid!");
        return result;
    }

    uint64_t lba_size = stor_device->get_lba_size();
    uint64_t cluster_entry_per_lba = lba_size / sizeof(fat32_cluster_entry);

    fat32_cluster_entry* cluster_table = kmalloc(lba_size); // cluster entry table (or FAT)

    fat32_cluster_entry current_cluster = chain_start_cluster;
    
    size_t allocated_chain_count = 16;
    result.chain = kmalloc(allocated_chain_count * sizeof(uint32_t));

    while(true){
        uint32_t cluster_entry_offset_in_lba = current_cluster % cluster_entry_per_lba;

        // read cluster entry
        // calculate which lba it is in fat & read
        if(stor_device->read(inst->fat1_first_lba + (current_cluster / cluster_entry_per_lba), 1, (uint8_t*)(cluster_table)) == FAIL){
            udbP("FAT ERROR: Failed to read FAT header!");
            kfree(cluster_table);
            kfree(result.chain);
            result.chain = nullptr;
            result.chain_len = 0;
            return result;
        }
        
        // add to list
        if(result.chain_len < allocated_chain_count){
            // size is enough
            result.chain[result.chain_len] = current_cluster;
            result.chain_len++;
        }else{
            // size is not enough
            allocated_chain_count += 16;
            result.chain = kreloc(result.chain, result.chain_len * sizeof(uint32_t), allocated_chain_count * sizeof(uint32_t));
            result.chain[result.chain_len] = current_cluster;
            result.chain_len++;
        }

        // go to next
        if(FAT32_CHECK_IS_CLUSTER_ALLOCATED(cluster_table[cluster_entry_offset_in_lba])){
            current_cluster = cluster_table[cluster_entry_offset_in_lba];
        }else{
            break; // no any more cluster left
        }
    }

    kfree(cluster_table);
    return result;
}

// NOTE: first_allocated_cluster can be nullptr
uos_result fat32_allocate_clusters(fat32_instance* inst, udevice_storage_function_pointers* stor_device, uint32_t chain_end_cluster, uint32_t* first_allocated_cluster,  size_t new_cluster_allocation_count, bool zero_clusters){
    if(new_cluster_allocation_count == 0){
        udbP("FAT ERROR: 0 new allocation is not allowed!");
        return FAIL;
    }

    fat32_cluster_entry* cluster_table = kmalloc(stor_device->get_lba_size()); // cluster entry table (or FAT)
    uint64_t cluster_entry_per_lba = stor_device->get_lba_size() / sizeof(fat32_cluster_entry);
    
    uint32_t current_cluster = 0; // current read cluster in buffer (THIS IS CLUSTER NUMBER NOT THE SECTOR NUMBER)
    if(inst->FSInfo.FSI_Nxt_Free != FAT32_FSINFO_UNKNOWN){
        current_cluster = (inst->FSInfo.FSI_Nxt_Free / cluster_entry_per_lba) * cluster_entry_per_lba;
    }
    size_t found_free_cluster_count = 0;
    bool hinted_cluster_is_used = false;
    uint32_t* free_clusters = kmalloc(new_cluster_allocation_count * sizeof(uint32_t));
    new_cluster_allocation_count++; // last one is for capturing last free
    while (found_free_cluster_count < new_cluster_allocation_count)
    {
        // read cluster entry
        // calculate which lba it is in fat & read
        if(stor_device->read(inst->fat1_first_lba + (current_cluster / cluster_entry_per_lba), 1, (uint8_t*)(cluster_table)) == FAIL){
            udbP("FAT ERROR: Failed to read FAT!");
            kfree(cluster_table);
            kfree(free_clusters);
            return FAIL;
        }

        // find free clusters to add to chain
        for(size_t i = 0; i < cluster_entry_per_lba; i++){
            if(cluster_table[i] == FAT32_CLUSTER_FREE){
                if(found_free_cluster_count + 1  == new_cluster_allocation_count){
                    // catch last one write it to fsinfo
                    if(((current_cluster) + i) < inst->FSInfo.FSI_Nxt_Free || hinted_cluster_is_used){
                        inst->FSInfo.FSI_Nxt_Free = (current_cluster) + i;
                        if(inst->FSInfo.FSI_Free_Count != FAT32_FSINFO_UNKNOWN){
                            inst->FSInfo.FSI_Free_Count -= new_cluster_allocation_count - 1; // last captured free is not allocated so subtract it
                        }
                        // write back to storage
                        if(stor_device->write(inst->partition.start_lba + inst->bpb.BPB_FSInfo, 1, (uint8_t*)&(inst->FSInfo)) == FAIL){
                            udbP("FAT ERROR: Failed to writeback FSInfo header!");
                            kfree(cluster_table);
                            kfree(free_clusters);
                            return FAIL;
                        }
                    }
                    found_free_cluster_count++;

                    // free clusters found
                    new_cluster_allocation_count--; // release captured last free
                    break; // now go to allocation

                }else{
                    free_clusters[found_free_cluster_count] = (current_cluster) + i;
                    if(free_clusters[found_free_cluster_count] == inst->FSInfo.FSI_Nxt_Free){
                        hinted_cluster_is_used = true;
                    }
                    found_free_cluster_count++;
                }
            }
        }

        if(found_free_cluster_count >= new_cluster_allocation_count){
            break; // go to allocation
        }

        // go next table
        current_cluster += cluster_entry_per_lba;
        if(current_cluster >= inst->data_cluster_count + 2){
            udbP("FAT ERROR: No free cluster left in volume!");
            kfree(cluster_table);
            kfree(free_clusters);
            return FAIL;
        }
    }

    uint8_t* zeroed_cluster = nullptr;
    if(zero_clusters){
        zeroed_cluster = kmalloc(stor_device->get_lba_size() * inst->bpb.sector_per_cluster);
        memset(zeroed_cluster, 0x0, stor_device->get_lba_size() * inst->bpb.sector_per_cluster);
    }

    // allocation
    uint32_t last_set_chain_member = chain_end_cluster;
    if(!FAT32_CHECK_IS_CLUSTER_ALLOCATED(last_set_chain_member)){
        last_set_chain_member = free_clusters[0];
    }
    uint32_t first_clus = last_set_chain_member;
    for(size_t i = 0; i < new_cluster_allocation_count; i++){
        if(last_set_chain_member / cluster_entry_per_lba != current_cluster / cluster_entry_per_lba){
            // current sector is not correct
            // write back current sector
            if(stor_device->write(inst->fat1_first_lba + (current_cluster / cluster_entry_per_lba), 1, (uint8_t*)(cluster_table)) == FAIL){
                udbP("FAT ERROR: Failed to writeback FAT!");
                kfree(cluster_table);
                kfree(free_clusters);
                if(zeroed_cluster != nullptr){
                    kfree(zeroed_cluster);
                }
                return FAIL;
            }
            if(stor_device->write(inst->fat2_first_lba + (current_cluster / cluster_entry_per_lba), 1, (uint8_t*)(cluster_table)) == FAIL){
                udbP("FAT ERROR: Failed to writeback FAT2!");
                kfree(cluster_table);
                kfree(free_clusters);
                if(zeroed_cluster != nullptr){
                    kfree(zeroed_cluster);
                }
                return FAIL;
            }
            // read correct sector
            current_cluster = last_set_chain_member;
            if(stor_device->read(inst->fat1_first_lba + (current_cluster / cluster_entry_per_lba), 1, (uint8_t*)(cluster_table)) == FAIL){
                udbP("FAT ERROR: Failed to read FAT!");
                kfree(cluster_table);
                kfree(free_clusters);
                if(zeroed_cluster != nullptr){
                    kfree(zeroed_cluster);
                }
                return FAIL;
            }
        }
        
        // zero clusters if required
        if(zero_clusters){
            if(stor_device->write(inst->data_first_lba + ((last_set_chain_member - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(zeroed_cluster)) == FAIL){
                udbP("FAT ERROR: Failed to write directory cluster!");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(cluster_table);
                kfree(free_clusters);
                if(zeroed_cluster != nullptr){
                    kfree(zeroed_cluster);
                }
                return FAIL;
            }
        }

        if(i + 1 != new_cluster_allocation_count){
            // not the last cluster
            cluster_table[last_set_chain_member % cluster_entry_per_lba] = free_clusters[i]; // give next
            last_set_chain_member = free_clusters[i];
        }else{
            // last cluster
            cluster_table[last_set_chain_member % cluster_entry_per_lba] = FAT32_CLUSTER_CHAIN_END_MIN; // end
        }

    }

    // write back last current sector
    if(stor_device->write(inst->fat1_first_lba + (current_cluster / cluster_entry_per_lba), 1, (uint8_t*)(cluster_table)) == FAIL){
        udbP("FAT ERROR: Failed to writeback FAT!");
        kfree(cluster_table);
        kfree(free_clusters);
        if(zeroed_cluster != nullptr){
            kfree(zeroed_cluster);
        }
        return FAIL;
    }
    if(stor_device->write(inst->fat2_first_lba + (current_cluster / cluster_entry_per_lba), 1, (uint8_t*)(cluster_table)) == FAIL){
        udbP("FAT ERROR: Failed to writeback FAT2!");
        kfree(cluster_table);
        kfree(free_clusters);
        if(zeroed_cluster != nullptr){
            kfree(zeroed_cluster);
        }
        return FAIL;
    }

    kfree(cluster_table);
    kfree(free_clusters);
    if(zeroed_cluster != nullptr){
        kfree(zeroed_cluster);
    }
    if(first_allocated_cluster != nullptr){
        *first_allocated_cluster = first_clus;
    }
    return SUCCESS; // nothing failed
}

// frees clusters
// NOTE: BE SURE chain_start_cluster IS NOT THE FIRST CLUSTER OF THE CHAIN WHEN middle_free IS TRUE!
uos_result fat32_free_clusters(fat32_instance* inst, udevice_storage_function_pointers* stor_device, uint32_t chain_start_cluster, bool middle_free){
    // find chain members
    fat32_cluster_chain ch = {};

    if(middle_free){
        ch = fat32_get_chain(inst, stor_device, chain_start_cluster - 1);
    }else{
        ch = fat32_get_chain(inst, stor_device, chain_start_cluster);
    }
    
    if(ch.chain == nullptr || ch.chain_len == 0){
        udbP("FAT ERROR: Start chain is not valid!");
        return FAIL;
    }

    // set them as free
    fat32_cluster_entry* cluster_table = kmalloc(stor_device->get_lba_size()); // cluster entry table (or FAT)
    uint64_t cluster_entry_per_lba = stor_device->get_lba_size() / sizeof(fat32_cluster_entry);
    
    uint32_t current_cluster_lba = UINT32_MAX; // current fat lba (UINT32_MAX is a impossible current cluster)

    uint32_t lowest_freed_cluster = UINT32_MAX; // to update fsinfo

    for(size_t i = 0; i <  ch.chain_len; i++){
        if(!FAT32_CHECK_IS_CLUSTER_ALLOCATED(ch.chain[i])){
            udbP("FAT ERROR: Chain has invalid member! (THIS MESSAGE SHOULD NOT APPEAR IN ANY WAY)");
            kfree(ch.chain);
            kfree(cluster_table);
            return FAIL;
        }

        // read or write-back current if required
        uint32_t required_cluster_lba = (ch.chain[i]) / cluster_entry_per_lba;
        if(current_cluster_lba != required_cluster_lba){

            // write back current if set
            if(current_cluster_lba != UINT32_MAX){
                if(stor_device->write(inst->fat1_first_lba + current_cluster_lba, 1, (uint8_t*)(cluster_table)) == FAIL){
                    udbP("FAT ERROR: Failed to write FAT!");
                    kfree(ch.chain);
                    kfree(cluster_table);
                    return FAIL;
                }

                if(stor_device->write(inst->fat2_first_lba + current_cluster_lba, 1, (uint8_t*)(cluster_table)) == FAIL){
                    udbP("FAT ERROR: Failed to write FAT2!");
                    kfree(ch.chain);
                    kfree(cluster_table);
                    return FAIL;
                }
            }

            // read required
            if(stor_device->read(inst->fat1_first_lba + required_cluster_lba, 1, (uint8_t*)(cluster_table)) == FAIL){
                udbP("FAT ERROR: Failed to read FAT!");
                kfree(ch.chain);
                kfree(cluster_table);
                return FAIL;
            }
            current_cluster_lba = required_cluster_lba;
        }

        // set chain member as free or set as chain end
        uint32_t index_in_lba = (ch.chain[i]) % cluster_entry_per_lba;
        if(middle_free){
            if(i == 0){
                cluster_table[index_in_lba] = FAT32_CLUSTER_CHAIN_END_MIN; // set as end of chain
                middle_free = false;
                continue;
            }
        }
        cluster_table[index_in_lba] = FAT32_CLUSTER_FREE;
        if(ch.chain[i] < lowest_freed_cluster){
            lowest_freed_cluster = ch.chain[i];
        }
    }
    // write back last read one if set
    if(current_cluster_lba != UINT32_MAX){
        if(stor_device->write(inst->fat1_first_lba + current_cluster_lba, 1, (uint8_t*)(cluster_table)) == FAIL){
            udbP("FAT ERROR: Failed to write FAT!");
            kfree(ch.chain);
            kfree(cluster_table);
            return FAIL;
        }
        if(stor_device->write(inst->fat2_first_lba + current_cluster_lba, 1, (uint8_t*)(cluster_table)) == FAIL){
            udbP("FAT ERROR: Failed to write FAT2!");
            kfree(ch.chain);
            kfree(cluster_table);
            return FAIL;
        }
    }

    // update fsinfo
    if(lowest_freed_cluster < inst->FSInfo.FSI_Nxt_Free){
        inst->FSInfo.FSI_Nxt_Free = lowest_freed_cluster;
    }
    if(inst->FSInfo.FSI_Free_Count != FAT32_FSINFO_UNKNOWN){
        inst->FSInfo.FSI_Free_Count += ch.chain_len; // add freed cluster count
    }
    // write back to storage
    if(stor_device->write(inst->partition.start_lba + inst->bpb.BPB_FSInfo, 1, (uint8_t*)&(inst->FSInfo)) == FAIL){
        udbP("FAT ERROR: Failed to writeback FSInfo header!");
        kfree(cluster_table);
        kfree(ch.chain);
        return FAIL;
    }

    kfree(ch.chain);
    kfree(cluster_table);
    return SUCCESS;
}

typedef struct
{
    fat32_dir_entry entry;
    size_t index_in_dir_list;
    wide_char_t wide_name[256]; // 255 char + nullterm
    bool sfn_match;
}fat32_entry_result;

fat32_entry_result fat32_find_entry(fat32_instance* inst, fat32_dir_entry* parent, const wide_char_t* entry_name, char* sfn_name){
    //udbP("searching for name:");
    //udbPw(entry_name);
    fat32_entry_result result = {};
    result.entry.DIR_Name[0] = FAT32_DIR_ENTRY_FREE; // not found
    result.sfn_match = false;

    // return self if empty
    if(*entry_name == '\0' || !wstrcmp(entry_name, u"/")){
        memcpy(&(result.entry), parent, sizeof(fat32_dir_entry));
        return result;
    }

    if(!(parent->DIR_Attr & FAT32_ATTR_DIRECTORY_BIT)){
        udbP("FAT ERROR: Parent is not a directory!");
        return result;
    }

    // open device
    udevice_storage_function_pointers* stor_device = _open_storage_device_(inst->storage_device);

    if(stor_device == nullptr){
        udbP("FAT ERROR: Failed to open storage device!");
        return result;
    }

    uint64_t lba_size = stor_device->get_lba_size();
    uint64_t cluster_entry_per_lba = lba_size / sizeof(fat32_cluster_entry);
    uint64_t dir_entry_count_per_cluster = (lba_size * inst->bpb.sector_per_cluster) / sizeof(fat32_dir_entry);

    fat32_cluster_entry* cluster_table = kmalloc(lba_size); // cluster entry table (or FAT)
    fat32_dir_entry* current_table = kmalloc(lba_size * inst->bpb.sector_per_cluster); // cluster
    fat32_long_dir_entry* long_current_table = (fat32_long_dir_entry*)current_table;
    wide_char_t* current_lfn_name = kmalloc(256 * sizeof(wide_char_t)); // + nullterm


    fat32_cluster_entry current_cluster = (parent->DIR_FstClusLO | (parent->DIR_FstClusHI << 16));
    size_t long_name_len = 0;
    uint8_t current_lfn_chksum = 0;
    uint8_t expected_lfn_id = 0;

    // dir search
    while(true){
        uint32_t cluster_entry_offset_in_lba = current_cluster % cluster_entry_per_lba;

        // read cluster entry
        // calculate which lba it is in fat & read
        if(stor_device->read(inst->fat1_first_lba + (current_cluster / cluster_entry_per_lba), 1, (uint8_t*)(cluster_table)) == FAIL){
            udbP("FAT ERROR: Failed to read FAT header!");
            if(uobject_close_object(inst->storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            kfree(cluster_table);
            kfree(current_table);
            kfree(current_lfn_name);
            return result;
        }

        // cluster dir search
        // read cluster (directory entries)
        if(stor_device->read(inst->data_first_lba + ((current_cluster - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
            udbP("FAT ERROR: Failed to read directory cluster!");
            if(uobject_close_object(inst->storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            kfree(cluster_table);
            kfree(current_table);
            kfree(current_lfn_name);
            return result;
        }
            
        for(uint32_t i = 0; i < dir_entry_count_per_cluster; i++){
            if(current_table[i].DIR_Name[0] == FAT32_DIR_ENTRY_FREE_START){
                // no any entries available further
                // udbP("FAT ERROR: Entry does not exists!");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(cluster_table);
                kfree(current_table);
                kfree(current_lfn_name);
                return result;
            }else if(current_table[i].DIR_Name[0] != FAT32_DIR_ENTRY_FREE){
                // check long name
                if(current_table[i].DIR_Attr == FAT32_ATTR_LONG_FILE_NAME){
                    if(long_name_len){
                        if(long_current_table[i].LDIR_Ord == expected_lfn_id && long_current_table[i].LDIR_Chksum == current_lfn_chksum){
                            long_name_len = prepend_lfn(&long_current_table[i], current_lfn_name, long_name_len);
                            expected_lfn_id--;
                        }else{
                            // possiable fs corruption
                            udbP("FAT ERROR: LFN mismatch0 found! Maybe filesystem is corrupted?");
                            if(uobject_close_object(inst->storage_device) == FAIL){
                                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                            }
                            kfree(cluster_table);
                            kfree(current_table);
                            kfree(current_lfn_name);
                            return result;
                        }
                    }else{
                        if(long_current_table[i].LDIR_Ord & FAT32_LFN_ID_FLAG_BEGIN){
                            long_name_len = prepend_lfn(&long_current_table[i], current_lfn_name, long_name_len);
                            current_lfn_chksum = long_current_table[i].LDIR_Chksum;
                            expected_lfn_id = (long_current_table[i].LDIR_Ord & ~FAT32_LFN_ID_FLAG_BEGIN) - 1;
                        }else{
                            // possiable fs corruption
                            udbP("FAT ERROR: LFN mismatch1 found! Maybe filesystem is corrupted?");
                            if(uobject_close_object(inst->storage_device) == FAIL){
                                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                            }
                            kfree(cluster_table);
                            kfree(current_table);
                            kfree(current_lfn_name);
                            return result;
                        }
                    }
                }else{
                    if(long_name_len){
                        if(current_lfn_chksum == fat32_calculate_lfn_chksum(current_table[i].DIR_Name) && expected_lfn_id == 0){
                            char* current_name = kmalloc(12 * sizeof(char));
                            memcpy(current_name, current_table[i].DIR_Name, 11);
                            current_name[11] = '\0';
                            if(!wstrcmp(current_lfn_name, entry_name) || !strcmp(current_name, sfn_name)){
                                // found!
                                memcpy(&(result.entry), &(current_table[i]), sizeof(fat32_dir_entry));
                                memcpy(result.wide_name, current_lfn_name, 256 * sizeof(wide_char_t));
                                result.index_in_dir_list = i;
                                result.sfn_match = !strcmp(current_name, sfn_name); // for knowing what is conflicting
                                if(uobject_close_object(inst->storage_device) == FAIL){
                                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                                }
                                kfree(cluster_table);
                                kfree(current_table);
                                kfree(current_lfn_name);
                                kfree(current_name);
                                return result;
                            }else{
                                // not found
                                long_name_len = 0;
                                current_lfn_chksum = 0;
                                expected_lfn_id = 0;
                                kfree(current_name);
                                continue;
                            }
                        }else{
                            // possiable fs corruption
                            udbP("FAT ERROR: LFN mismatch2 found! Maybe filesystem is corrupted?");
                            if(uobject_close_object(inst->storage_device) == FAIL){
                                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                            }
                            kfree(cluster_table);
                            kfree(current_table);
                            kfree(current_lfn_name);
                            return result;
                        }
                    }else{
                        char* short_name = kmalloc(256 * sizeof(char));
                        char* current_name = kmalloc(12 * sizeof(char));
                        memcpy(current_name, current_table[i].DIR_Name, 11);
                        current_name[11] = '\0';
                        utf16_to_char_buf(entry_name, short_name);
                        if(!strcmp(short_name, current_name) || !strcmp(current_name, sfn_name)){
                            // found!
                            memcpy(&(result.entry), &(current_table[i]), sizeof(fat32_dir_entry));
                            char_to_utf16_buf(current_name, result.wide_name);
                            result.index_in_dir_list = i;
                            result.sfn_match = !strcmp(current_name, sfn_name); // for knowing what is conflicting
                            if(uobject_close_object(inst->storage_device) == FAIL){
                                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                            }
                            kfree(cluster_table);
                            kfree(current_table);
                            kfree(current_lfn_name);
                            kfree(short_name);
                            kfree(current_name);
                            return result;
                        }else{
                            // not found
                            kfree(short_name);
                            kfree(current_name);
                            continue;
                        }
                    }
                }
            }
        }
        
        // not found go to next dir entry cluster
        if(FAT32_CHECK_IS_CLUSTER_ALLOCATED(cluster_table[cluster_entry_offset_in_lba])){
            current_cluster = cluster_table[cluster_entry_offset_in_lba];
        }else{
            break; // no any more cluster left
        }

    }

    // not found
    if(uobject_close_object(inst->storage_device) == FAIL){
        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
    }
    kfree(cluster_table);
    kfree(current_table);
    kfree(current_lfn_name);
    return result;
}

uos_result fat32_update_entry(fat32_instance* inst, fat32_dir_entry* parent, size_t index_in_dir_list, fat32_dir_entry* entry){
    if(!(parent->DIR_Attr & FAT32_ATTR_DIRECTORY_BIT)){
        udbP("FAT ERROR: Parent is not a directory!");
        return FAIL;
    }

    // open device
    udevice_storage_function_pointers* stor_device = _open_storage_device_(inst->storage_device);

    if(stor_device == nullptr){
        udbP("FAT ERROR: Failed to open storage device!");
        return FAIL;
    }


    uint64_t lba_size = stor_device->get_lba_size();
    uint64_t dir_entry_count_per_cluster = (lba_size * inst->bpb.sector_per_cluster) / sizeof(fat32_dir_entry);

    uint32_t dir_clus_start = (uint32_t)(parent->DIR_FstClusLO) || ((uint32_t)parent->DIR_FstClusHI << 16);
    fat32_cluster_chain ch = fat32_get_chain(inst, stor_device, dir_clus_start);

    if(ch.chain == nullptr){
        udbP("FAT ERROR: Directory doestn have a dir table!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    size_t entry_cluster_idx = index_in_dir_list / dir_entry_count_per_cluster;

    if(!(entry_cluster_idx < ch.chain_len)){
        udbP("FAT ERROR: Entry is not in bounds of dir table!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        kfree(ch.chain);
        return FAIL;
    }

    fat32_dir_entry* current_table = kmalloc(lba_size * inst->bpb.sector_per_cluster); // 1 cluster
    // read cluster (directory entries)
    if(stor_device->read(inst->data_first_lba + ((ch.chain[entry_cluster_idx] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
        udbP("FAT ERROR: Failed to read directory cluster!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        kfree(ch.chain);
        kfree(current_table);
        return FAIL;
    }

    // update
    uint64_t entry_index_in_cluster = index_in_dir_list % dir_entry_count_per_cluster;
    current_table[entry_index_in_cluster] = *entry;

    // write back cluster (directory entries)
    if(stor_device->write(inst->data_first_lba + ((ch.chain[entry_cluster_idx] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
        udbP("FAT ERROR: Failed to write directory cluster!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        kfree(ch.chain);
        kfree(current_table);
        return FAIL;
    }

    kfree(ch.chain);
    kfree(current_table);
    if(uobject_close_object(inst->storage_device) == FAIL){
        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        return FAIL;
    }
    return SUCCESS;
}

// NOTE: THIS FUNCTION ONLY DELETES ENTRY NOT THE LINKED CLUSTER CHAIN!
uos_result fat32_delete_entry(fat32_instance* inst, fat32_dir_entry* parent, size_t deleting_index_in_dir_list){
    // open device
    udevice_storage_function_pointers* stor_device = _open_storage_device_(inst->storage_device);

    if(stor_device == nullptr){
        udbP("FAT ERROR: Failed to open storage device!");
        return FAIL;
    }

    // delete entries (search lfn entries backwardly)
    uint64_t dir_entry_count_per_cluster = (stor_device->get_lba_size() * inst->bpb.sector_per_cluster) / sizeof(fat32_dir_entry);
    uint32_t parent_clus_chain_start = (uint32_t)(parent->DIR_FstClusLO) | ((uint32_t)parent->DIR_FstClusHI << 16);
    int64_t current_entry_idx = deleting_index_in_dir_list;
    uint64_t current_dir_table_cluster_idx = (current_entry_idx + 1) / dir_entry_count_per_cluster; // +1 for checking is this entry is free start (reads next entry's cluster)
    fat32_cluster_chain ch = fat32_get_chain(inst, stor_device, parent_clus_chain_start);
    if(ch.chain == nullptr){
        udbP("FAT ERROR: Failed to read directory cluster chain!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL; 
    }
    fat32_dir_entry* current_table = kmalloc(stor_device->get_lba_size() * inst->bpb.sector_per_cluster); // cluster
    bool read_cluster_updated = false;

    // read cluster
    if(stor_device->read(inst->data_first_lba + ((ch.chain[current_dir_table_cluster_idx] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
        udbP("FAT ERROR: Failed to read directory cluster!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        kfree(current_table);
        return FAIL;
    }

    // check is that is a free start entry
    uint8_t free_type = FAT32_DIR_ENTRY_FREE;
    uint32_t relative_current_entry_idx = (current_entry_idx + 1) % dir_entry_count_per_cluster;
    if(current_table[relative_current_entry_idx].DIR_Name[0] == FAT32_DIR_ENTRY_FREE_START){
        free_type = FAT32_DIR_ENTRY_FREE_START;
    }

    // read sfn entry's cluster (if not same as next one)
    // NOTE: We dont need to write back last read cluster because we didnt changed it
    current_dir_table_cluster_idx = (current_entry_idx) / dir_entry_count_per_cluster;
    relative_current_entry_idx = (current_entry_idx) % dir_entry_count_per_cluster;
    if(stor_device->read(inst->data_first_lba + ((ch.chain[current_dir_table_cluster_idx] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
        udbP("FAT ERROR: Failed to read directory cluster!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        kfree(current_table);
        return FAIL;
    }

    // delete sfn entry
    current_table[relative_current_entry_idx].DIR_Name[0] = free_type;
    read_cluster_updated = true;
    current_entry_idx--; // go last lfn entry


    while(current_entry_idx >= 0)
    {
        uint64_t required_dir_table_cluster_index = current_entry_idx / dir_entry_count_per_cluster;
        if(required_dir_table_cluster_index > ch.chain_len){
            break; // no any more entry left
        }
        if(required_dir_table_cluster_index != current_dir_table_cluster_idx){
            // write-back current if needed
            if(read_cluster_updated){
                if(stor_device->write(inst->data_first_lba + ((ch.chain[current_dir_table_cluster_idx] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
                    udbP("FAT ERROR: Failed to write directory cluster!");
                    if(uobject_close_object(inst->storage_device) == FAIL){
                        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                    }
                    kfree(current_table);
                    kfree(ch.chain);
                    return FAIL;
                }
                read_cluster_updated = false;
            }

            // read required
            current_dir_table_cluster_idx = required_dir_table_cluster_index;
            if(stor_device->read(inst->data_first_lba + ((ch.chain[current_dir_table_cluster_idx] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
                udbP("FAT ERROR: Failed to read directory cluster!");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(current_table);
                kfree(ch.chain);
                return FAIL;
            }
        }

        relative_current_entry_idx = current_entry_idx % dir_entry_count_per_cluster;

        if(current_table[relative_current_entry_idx].DIR_Name[0] == FAT32_DIR_ENTRY_FREE || current_table[relative_current_entry_idx].DIR_Name[0] == FAT32_DIR_ENTRY_FREE_START){
            break; // no entry left to search
        }
        if(current_table[relative_current_entry_idx].DIR_Attr == FAT32_ATTR_LONG_FILE_NAME){
            current_table[relative_current_entry_idx].DIR_Name[0] = free_type;
            read_cluster_updated = true;
            current_entry_idx--; // go last lfn entry
        }else{
            break; // no entry left to search
        }
    }

    // write-back current if needed
    if(read_cluster_updated){
        if(stor_device->write(inst->data_first_lba + ((ch.chain[current_dir_table_cluster_idx] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
            udbP("FAT ERROR: Failed to write directory cluster!");
            if(uobject_close_object(inst->storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            kfree(current_table);
            kfree(ch.chain);
            return FAIL;
        }
        read_cluster_updated = false;
    }

    kfree(current_table);
    kfree(ch.chain);
    if(uobject_close_object(inst->storage_device) == FAIL){
        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        return FAIL;
    }
    return SUCCESS;
}

// returns  UINT32_MAX if not found
uint32_t fat32_find_continious_free_entries(fat32_instance* inst, fat32_dir_entry parent, size_t len){
     if(!(parent.DIR_Attr & FAT32_ATTR_DIRECTORY_BIT)){
        udbP("FAT ERROR: Parent is not a directory!");
        return UINT32_MAX;
    }

    // open device
    udevice_storage_function_pointers* stor_device = _open_storage_device_(inst->storage_device);

    if(stor_device == nullptr){
        udbP("FAT ERROR: Failed to open storage device!");
        return UINT32_MAX;
    }

    uint64_t lba_size = stor_device->get_lba_size();
    uint64_t cluster_entry_per_lba = lba_size / sizeof(fat32_cluster_entry);
    uint64_t dir_entry_count_per_cluster = (lba_size * inst->bpb.sector_per_cluster) / sizeof(fat32_dir_entry);

    fat32_cluster_entry* cluster_table = kmalloc(lba_size); // cluster entry table (or FAT)
    fat32_dir_entry* current_table = kmalloc(lba_size * inst->bpb.sector_per_cluster); // cluster

    fat32_cluster_entry current_cluster = (parent.DIR_FstClusLO | (parent.DIR_FstClusHI << 16));
    uint32_t checked_cluster_count = 0;
    uint32_t free_start = UINT32_MAX;
    uint32_t continuous_free_cluster_count = 0;

    // dir search
    while(true){
        uint32_t cluster_entry_offset_in_lba = current_cluster % cluster_entry_per_lba;

        // read cluster entry
        // calculate which lba it is in fat & read
        if(stor_device->read(inst->fat1_first_lba + (current_cluster / cluster_entry_per_lba), 1, (uint8_t*)(cluster_table)) == FAIL){
            udbP("FAT ERROR: Failed to read FAT header!");
            if(uobject_close_object(inst->storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            kfree(cluster_table);
            kfree(current_table);
            return UINT32_MAX;
        }

        // cluster dir search
        // read cluster (directory entries)
        if(stor_device->read(inst->data_first_lba + ((current_cluster - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
            udbP("FAT ERROR: Failed to read directory cluster!");
            if(uobject_close_object(inst->storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            kfree(cluster_table);
            kfree(current_table);
            return UINT32_MAX;
        }
            
        for(uint32_t i = 0; i < dir_entry_count_per_cluster; i++){
            if(current_table[i].DIR_Name[0] == FAT32_DIR_ENTRY_FREE){
                if(continuous_free_cluster_count){
                    continuous_free_cluster_count++;
                }else{
                    free_start = (checked_cluster_count * dir_entry_count_per_cluster) + i;
                    continuous_free_cluster_count++;
                }
            }else if(current_table[i].DIR_Name[0] == FAT32_DIR_ENTRY_FREE_START){
                if(dir_entry_count_per_cluster - i >= len){
                    if(continuous_free_cluster_count){
                        if(uobject_close_object(inst->storage_device) == FAIL){
                            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                        }
                        kfree(cluster_table);
                        kfree(current_table);
                        return free_start; // use already found free start if available
                    }else{
                        if(uobject_close_object(inst->storage_device) == FAIL){
                            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                        }
                        kfree(cluster_table);
                        kfree(current_table);
                        return (checked_cluster_count * dir_entry_count_per_cluster) + i; // enough space available
                    }
                }else{
                    continuous_free_cluster_count += dir_entry_count_per_cluster - i;
                    break;
                }
            }else{
                free_start = UINT32_MAX;
                continuous_free_cluster_count = 0;
            }

            if(continuous_free_cluster_count >= len){
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(cluster_table);
                kfree(current_table);
                return free_start;
            }
        }

        // this double check is for edgecase
        if(continuous_free_cluster_count >= len){
            if(uobject_close_object(inst->storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            kfree(cluster_table);
            kfree(current_table);
            return free_start;
        }
        
        // not found go to next dir entry cluster
        if(FAT32_CHECK_IS_CLUSTER_ALLOCATED(cluster_table[cluster_entry_offset_in_lba])){
            current_cluster = cluster_table[cluster_entry_offset_in_lba];
            checked_cluster_count++;
        }else{
            // no any more cluster left
            // try to allocate new cluster
            if((checked_cluster_count * dir_entry_count_per_cluster) <= UINT16_MAX){
                if(fat32_allocate_clusters(inst, stor_device, current_cluster, nullptr, 1, true) != SUCCESS){
                    udbP("FAT ERROR: Not enough entry left in directory! H0");
                    if(uobject_close_object(inst->storage_device) == FAIL){
                        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                    }
                    kfree(cluster_table);
                    kfree(current_table);
                    return UINT32_MAX;
                }
                // we dont have updated version here so we have to readback the fat cluster entries

                if(stor_device->read(inst->fat1_first_lba + (current_cluster / cluster_entry_per_lba), 1, (uint8_t*)(cluster_table)) == FAIL){
                    udbP("FAT ERROR: Failed to read FAT header!");
                    if(uobject_close_object(inst->storage_device) == FAIL){
                        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                    }
                    kfree(cluster_table);
                    kfree(current_table);
                    return UINT32_MAX;
                }
                current_cluster = cluster_table[cluster_entry_offset_in_lba];
                checked_cluster_count++;
            }else{
                udbP("FAT ERROR: Not enough entry left in directory! H1");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(cluster_table);
                kfree(current_table);
                return UINT32_MAX;
            }
        }

    }

    // not found
    udbP("FAT ERROR: This should not reach here!");
    if(uobject_close_object(inst->storage_device) == FAIL){
        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
    }
    kfree(cluster_table);
    kfree(current_table);
    return UINT32_MAX;
}

// empty sfn tries to find a suitable random sfn name
uos_result fat32_create_dir_entry(fat32_instance* inst, fat32_entry_result parent_entry, const wide_char_t* entry_name, char* sfn_name, uint8_t attrbs, uint32_t assigned_cluster, uint32_t file_size){

    char random_sfn_name[11];
    bool use_random_sfn = false;
    if(*sfn_name == '\0' || sfn_name == nullptr){
        generate_random_short_name(random_sfn_name);
        sfn_name = random_sfn_name;
        use_random_sfn = true;
    }
    
    // try to find a suitable sfn name
    fat32_entry_result found;
    bool filename_suitable = false;
    for(size_t i = 0; i < 8196; i++){
        found = fat32_find_entry(inst, &(parent_entry.entry), entry_name, sfn_name);
        if(!(found.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE || found.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE_START)){
            // found a match
            if(found.sfn_match){
                if(use_random_sfn){
                    generate_random_short_name(sfn_name);
                }else{
                    udbP("FAT ERROR: A entry with this short file name already exists!");
                    return FAIL; 
                }
            }else{
                udbP("FAT ERROR: A entry with this long file name already exists!");
                return FAIL;
            }
        }else{
            // no confliction
            filename_suitable = true;
            break;
        }
    }

    if(!filename_suitable){
        udbP("FAT ERROR: Failed to find a suitable random sfnv!");
        return FAIL;
    }

    uint32_t name_len = wstrlen(entry_name, 256);
    uint32_t required_lfn_entry_count = (name_len + 12) / 13;

    uint32_t entry_start = fat32_find_continious_free_entries(inst, parent_entry.entry, required_lfn_entry_count + 1);
    if(entry_start == UINT32_MAX){
        udbP("FAT ERROR: Failed to create entry in parent directory!");
        return FAIL;
    }

    // open device
    udevice_storage_function_pointers* stor_device = _open_storage_device_(inst->storage_device);
    
    if(stor_device == nullptr){
        udbP("FAT ERROR: Failed to open storage device!");
        return FAIL;
    }

    fat32_cluster_entry* cluster_table = kmalloc(stor_device->get_lba_size()); // cluster entry table (or FAT)
    fat32_dir_entry* current_table = kmalloc(stor_device->get_lba_size() * inst->bpb.sector_per_cluster); // cluster
    fat32_long_dir_entry* long_current_table = (fat32_long_dir_entry*)current_table;

    uint64_t dir_entry_count_per_cluster = (stor_device->get_lba_size() * inst->bpb.sector_per_cluster) / sizeof(fat32_dir_entry);
    uint64_t cluster_entry_per_lba = stor_device->get_lba_size() / sizeof(fat32_cluster_entry);

    // write entries
    uint8_t sfn_chksum = fat32_calculate_lfn_chksum(sfn_name);
    size_t last_lfn_pos = 0;
    
    fat32_cluster_entry parent_dir_cluster_start = (parent_entry.entry.DIR_FstClusLO | (parent_entry.entry.DIR_FstClusHI << 16));
    fat32_cluster_chain chain = fat32_get_chain(inst, stor_device, parent_dir_cluster_start);

    if(chain.chain == nullptr){
        udbP("FAT ERROR: Failed to read cluster chain!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        kfree(current_table);
        kfree(cluster_table);
        return FAIL;
    }

    uint32_t current_read_relative_cluster = FAT32_CLUSTER_BAD_CLUSTER;
    for(size_t i = 0; i < required_lfn_entry_count + 1; i++){
        uint32_t current_relative_cluster = (entry_start + i) / dir_entry_count_per_cluster;
        uint32_t current_entry = (entry_start + i) % dir_entry_count_per_cluster;

        // read cluster if not already read cluster is matching
        if(current_read_relative_cluster != current_relative_cluster){
            if(current_read_relative_cluster != FAT32_CLUSTER_BAD_CLUSTER){
                // writeback current cluster if it is valid
                if(stor_device->write(inst->data_first_lba + ((chain.chain[current_read_relative_cluster] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
                    udbP("FAT ERROR: Failed to writeback directory cluster!");
                    if(uobject_close_object(inst->storage_device) == FAIL){
                        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                    }
                    kfree(cluster_table);
                    kfree(current_table);
                    kfree(chain.chain);
                    return FAIL;
                }
            }

            // read new cluster
            if(stor_device->read(inst->data_first_lba + ((chain.chain[current_relative_cluster] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
                udbP("FAT ERROR: Failed to read directory cluster!");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(cluster_table);
                kfree(current_table);
                kfree(chain.chain);
                return FAIL;
            }
            current_read_relative_cluster = current_relative_cluster;
        }

        if(i == 0){
            // first lfn entry
            long_current_table[current_entry].LDIR_Ord = required_lfn_entry_count | FAT32_LFN_ID_FLAG_BEGIN;
            long_current_table[current_entry].LDIR_FstClusLO = 0;
            long_current_table[current_entry].LDIR_Type = 0;
            long_current_table[current_entry].LDIR_Chksum = sfn_chksum;
            long_current_table[current_entry].LDIR_Attr = FAT32_ATTR_LONG_FILE_NAME;
            last_lfn_pos = put_in_lfn(&long_current_table[current_entry], entry_name, last_lfn_pos);
        }else if(i == required_lfn_entry_count){
            // last sfn entry
            current_table[current_entry].DIR_Attr = attrbs;
            current_table[current_entry].DIR_CrtDate = 0;
            current_table[current_entry].DIR_CrtTime = 0;
            current_table[current_entry].DIR_CrtTimeTenth = 0;
            current_table[current_entry].DIR_LstAccDate = 0;
            memcpy(current_table[current_entry].DIR_Name, sfn_name, 11);
            current_table[current_entry].DIR_NTRes = 0;
            current_table[current_entry].DIR_FileSize = file_size;
            current_table[current_entry].DIR_FstClusLO = assigned_cluster & 0xFFFF;
            current_table[current_entry].DIR_FstClusHI = assigned_cluster >> 16;
        }else{
            // middle lfn entries
            long_current_table[current_entry].LDIR_Ord = required_lfn_entry_count - i;
            long_current_table[current_entry].LDIR_FstClusLO = 0;
            long_current_table[current_entry].LDIR_Type = 0;
            long_current_table[current_entry].LDIR_Chksum = sfn_chksum;
            long_current_table[current_entry].LDIR_Attr = FAT32_ATTR_LONG_FILE_NAME;
            last_lfn_pos = put_in_lfn(&long_current_table[current_entry], entry_name, last_lfn_pos);
        }
    }

    // writeback last current cluster
    if(stor_device->write(inst->data_first_lba + ((chain.chain[current_read_relative_cluster] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
        udbP("FAT ERROR: Failed to writeback directory cluster!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        kfree(cluster_table);
        kfree(current_table);
        kfree(chain.chain);
        return FAIL;
    }

    if(uobject_close_object(inst->storage_device) == FAIL){
        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
    }
    kfree(cluster_table);
    kfree(current_table);
    kfree(chain.chain);
    return SUCCESS;
}

// forward dec for uos_result fat32_copy();
uos_result fat32_read(u_fs_interface* interface, const wide_char_t* file_path, size_t read_offset, size_t read_len, void* read_buffer);
uos_result fat32_write(u_fs_interface* interface, const wide_char_t* file_path, size_t write_offset, size_t write_len, void* write_buffer);
uos_result fat32_resize_file(u_fs_interface* interface, const wide_char_t* file_path, size_t new_size);

//  --**## fat32 fsi handlers ##**--
u_fs_file_info fat32_read_info(u_fs_interface* interface, const wide_char_t* path){
    u_fs_file_info result = {};
    result.flags = FSI_FLAG_INVALID_BIT;
    
    fat32_instance* inst = fat32_find_instance(interface->partition);
    if(inst == nullptr){
        udbP("FAT ERROR: File system interface error! H0");
        return result;
    }

    // find entry
    fat32_dir_entry root_dir = {};
    fat32_get_root_dir_virt_entry(inst, &root_dir);

    fat32_entry_result file_entry = {};
    memcpy(&(file_entry.entry), &root_dir, sizeof(fat32_dir_entry));

    uint64_t up_dir_count = w_parse_read_count(path, '/');

    wide_char_t read_part[256];
    for(size_t i = 0; i < up_dir_count; i++){
        w_parse_read(read_part, path, '/', i);

        if(read_part[0] == '\0'){
            continue; // empty skip
        }else{
            file_entry = fat32_find_entry(inst, &(file_entry.entry), read_part, "");
            if(file_entry.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE){
                udbP("FAT ERROR: File not found!");
                return result;
            }
        }
    }

    fat32_dir_entry parent = {};
    memcpy(&parent, &(file_entry.entry), sizeof(fat32_dir_entry));

    if(file_entry.entry.DIR_Attr & FAT32_ATTR_DIRECTORY_BIT){
        // find child count if it is a directory
        // open device
        uint32_t found_child_count = 0;
        udevice_storage_function_pointers* stor_device = _open_storage_device_(inst->storage_device);

        if(stor_device == nullptr){
            udbP("FAT ERROR: Failed to open storage device!");
            return result;
        }

        uint64_t lba_size = stor_device->get_lba_size();
        uint64_t cluster_entry_per_lba = lba_size / sizeof(fat32_cluster_entry);
        uint64_t dir_entry_count_per_cluster = (lba_size * inst->bpb.sector_per_cluster) / sizeof(fat32_dir_entry);

        fat32_cluster_entry* cluster_table = kmalloc(lba_size); // cluster entry table (or FAT)
        fat32_dir_entry* current_table = kmalloc(lba_size * inst->bpb.sector_per_cluster); // cluster
        fat32_long_dir_entry* long_current_table = (fat32_long_dir_entry*)current_table;
        wide_char_t* current_lfn_name = kmalloc(256 * sizeof(wide_char_t)); // + nullterm
        memset(current_lfn_name, 0x0, 256 *  sizeof(wide_char_t));

        fat32_cluster_entry current_cluster = (parent.DIR_FstClusLO | (parent.DIR_FstClusHI << 16));
        size_t long_name_len = 0;
        uint8_t current_lfn_chksum = 0;
        uint8_t expected_lfn_id = 0;

        // dir search
        while(true){
            uint32_t cluster_entry_offset_in_lba = current_cluster % cluster_entry_per_lba;

            // read cluster entry
            // calculate which lba it is in fat & read
            if(stor_device->read(inst->fat1_first_lba + (current_cluster / cluster_entry_per_lba), 1, (uint8_t*)(cluster_table)) == FAIL){
                udbP("FAT ERROR: Failed to read FAT header!");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(cluster_table);
                kfree(current_table);
                kfree(current_lfn_name);
                return result;
            }

            // cluster dir search
            // read cluster (directory entries)
            if(stor_device->read(inst->data_first_lba + ((current_cluster - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
                udbP("FAT ERROR: Failed to read directory cluster!");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(cluster_table);
                kfree(current_table);
                kfree(current_lfn_name);
                return result;
            }

            bool search_completed = false;
            for(uint32_t i = 0; i < dir_entry_count_per_cluster; i++){
                if(current_table[i].DIR_Name[0] == FAT32_DIR_ENTRY_FREE_START){
                    // no any entries available further
                    if(uobject_close_object(inst->storage_device) == FAIL){
                        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                    }
                    kfree(cluster_table);
                    kfree(current_table);
                    kfree(current_lfn_name);

                    // RETURN RESULT
                    search_completed = true;
                    break;
                }else if(current_table[i].DIR_Name[0] != FAT32_DIR_ENTRY_FREE){
                    // check long name
                    if(current_table[i].DIR_Attr == FAT32_ATTR_LONG_FILE_NAME){
                        if(long_name_len){
                            if(long_current_table[i].LDIR_Ord == expected_lfn_id && long_current_table[i].LDIR_Chksum == current_lfn_chksum){
                                long_name_len = prepend_lfn(&long_current_table[i], current_lfn_name, long_name_len);
                                expected_lfn_id--;
                            }else{
                                // possiable fs corruption
                                udbP("FAT ERROR: LFN mismatch0 found! Maybe filesystem is corrupted?");
                                if(uobject_close_object(inst->storage_device) == FAIL){
                                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                                }
                                kfree(cluster_table);
                                kfree(current_table);
                                kfree(current_lfn_name);
                                return result;
                            }
                        }else{
                            if(long_current_table[i].LDIR_Ord & FAT32_LFN_ID_FLAG_BEGIN){
                                long_name_len = prepend_lfn(&long_current_table[i], current_lfn_name, long_name_len);
                                current_lfn_chksum = long_current_table[i].LDIR_Chksum;
                                expected_lfn_id = (long_current_table[i].LDIR_Ord & ~FAT32_LFN_ID_FLAG_BEGIN) - 1;
                            }else{
                                // possiable fs corruption
                                udbP("FAT ERROR: LFN mismatch1 found! Maybe filesystem is corrupted?");
                                if(uobject_close_object(inst->storage_device) == FAIL){
                                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                                }
                                kfree(cluster_table);
                                kfree(current_table);
                                kfree(current_lfn_name);
                                return result;
                            }
                        }
                    }else{
                        if(long_name_len){
                            if(current_lfn_chksum == fat32_calculate_lfn_chksum(current_table[i].DIR_Name) && expected_lfn_id == 0){

                                found_child_count++;

                                // go to next entry
                                long_name_len = 0;
                                memset(current_lfn_name, 0x0, 256 *  sizeof(wide_char_t));
                                current_lfn_chksum = 0;
                                expected_lfn_id = 0;
                                continue;

                            }else{
                                // possiable fs corruption
                                udbP("FAT ERROR: LFN mismatch2 found! Maybe filesystem is corrupted?");
                                if(uobject_close_object(inst->storage_device) == FAIL){
                                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                                }
                                kfree(cluster_table);
                                kfree(current_table);
                                kfree(current_lfn_name);
                                return result;
                            }
                        }else{
                            found_child_count++;
                            
                            // go to next entry
                            continue;
                        }
                    }
                }
            }

            if(search_completed){
                break;
            }

            // not found go to next dir entry cluster
            if(FAT32_CHECK_IS_CLUSTER_ALLOCATED(cluster_table[cluster_entry_offset_in_lba])){
                current_cluster = cluster_table[cluster_entry_offset_in_lba];
            }else{
                break; // no any more cluster left
            }

        }

        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        kfree(cluster_table);
        kfree(current_table);
        kfree(current_lfn_name);
        
        result.file_size = 0;
        result.child_count = found_child_count;
        result.flags = 0; // clear invalid flag
    }else{
        // read file size if it is not a directory
        result.file_size = file_entry.entry.DIR_FileSize;
        result.child_count = 0;
        result.flags = 0; // clear invalid flag
    }

    // fill flags
    result.flags = FSI_FLAG_EXECUTE_BIT; // this is required for not locking down volume
    if(file_entry.entry.DIR_Attr & FAT32_ATTR_DIRECTORY_BIT){
        result.flags |= FSI_FLAG_DIRECTORY_BIT;
    }
    if(file_entry.entry.DIR_Attr & FAT32_ATTR_READ_ONLY_BIT){
        result.flags |= FSI_FLAG_RO_BIT;
    }
    if(file_entry.entry.DIR_Attr & FAT32_ATTR_HIDDEN_BIT){
        result.flags |= FSI_FLAG_HIDDEN_BIT;
    }

    return result;
}

uos_result fat32_create_file(u_fs_interface* interface, const wide_char_t* parent, const wide_char_t* file_name){
    if(fat32_check_name(file_name) != SUCCESS){
        udbP("FAT ERROR: Illegal name!");
        return FAIL;
    }
    
    fat32_instance* inst = fat32_find_instance(interface->partition);
    if(inst == nullptr){
        udbP("FAT ERROR: File system interface error! H0");
        return FAIL;
    }

    fat32_dir_entry root_dir = {};
    fat32_get_root_dir_virt_entry(inst, &root_dir);

    fat32_entry_result parent_entry = {};
    memcpy(&(parent_entry.entry), &root_dir, sizeof(fat32_dir_entry));

    uint64_t up_dir_count = w_parse_read_count(parent, '/');

    wide_char_t read_part[256];
    for(size_t i = 0; i < up_dir_count; i++){
        w_parse_read(read_part, parent, '/', i);

        if(read_part[0] == '\0'){
            continue; // empty skip
        }else{
            parent_entry = fat32_find_entry(inst, &(parent_entry.entry), read_part, "");
            if(parent_entry.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE){
                udbP("FAT ERROR: Parent file not found!");
                return FAIL;
            }
        }
    }

    // no sfn name for auto random sfn name
    if(fat32_create_dir_entry(inst, parent_entry, file_name, "", 0, FAT32_CLUSTER_FREE, 0) != SUCCESS){
        udbP("FAT ERROR: Failed to create new file!");
        return FAIL;
    }

    return SUCCESS;
}

uos_result fat32_create_dir(u_fs_interface* interface, const wide_char_t* parent, const wide_char_t* dir_name){
    if(fat32_check_name(dir_name) != SUCCESS){
        udbP("FAT ERROR: Illegal name!");
        return FAIL;
    }
    
    udbPs();
    udbP_STR("creating a dir: ");
    udbP_wSTR(dir_name);
    udbP_STR(" in : ");
    udbP_wSTR(parent);
    udbPe();

    fat32_instance* inst = fat32_find_instance(interface->partition);
    if(inst == nullptr){
        udbP("FAT ERROR: File system interface error! H0");
        return FAIL;
    }

    fat32_dir_entry root_dir = {};
    fat32_get_root_dir_virt_entry(inst, &root_dir);

    fat32_entry_result parent_entry = {};
    memcpy(&(parent_entry.entry), &root_dir, sizeof(fat32_dir_entry));

    uint64_t up_dir_count = w_parse_read_count(parent, '/');

    wide_char_t read_part[256];
    for(size_t i = 0; i < up_dir_count; i++){
        w_parse_read(read_part, parent, '/', i);

        if(read_part[0] == '\0'){
            continue; // empty skip
        }else{
            parent_entry = fat32_find_entry(inst, &(parent_entry.entry), read_part, "");
            if(parent_entry.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE){
                udbP("FAT ERROR: Parent directory not found!");
                return FAIL;
            }
        }
    }

    udevice_storage_function_pointers* stor_device = _open_storage_device_(inst->storage_device);
    fat32_cluster_entry dir_clus = FAT32_CLUSTER_FREE;
    if(fat32_allocate_clusters(inst, stor_device, FAT32_NEW_CLUSTER, &dir_clus, 1, true) != SUCCESS){
        udbP("FAT ERROR: Failed to create new directory!");
        if(uobject_close_object(inst->storage_device) != SUCCESS){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    // write . & .. entries
    fat32_dir_entry* dir_table = kmalloc(stor_device->get_lba_size() * inst->bpb.sector_per_cluster);
    memset(dir_table, 0x0, stor_device->get_lba_size() * inst->bpb.sector_per_cluster);
    
    // .
    dir_table[0].DIR_Name[0] = '.';
    for(size_t i = 1; i < 11; i++){
        dir_table[0].DIR_Name[i] = 0x20;
    }
    dir_table[0].DIR_Attr = FAT32_ATTR_DIRECTORY_BIT;
    dir_table[0].DIR_NTRes = 0;
    dir_table[0].DIR_CrtDate = 0;
    dir_table[0].DIR_CrtTime = 0;
    dir_table[0].DIR_CrtTimeTenth = 0;
    dir_table[0].DIR_WrtTime = 0;
    dir_table[0].DIR_WrtDate = 0;
    dir_table[0].DIR_LstAccDate = 0;
    dir_table[0].DIR_FstClusHI = (dir_clus >> 16) & 0xFFFF; // self
    dir_table[0].DIR_FstClusLO = dir_clus & 0xFFFF; // self
    dir_table[0].DIR_FileSize = 0;

    // ..
    dir_table[1].DIR_Name[0] = '.';
    dir_table[1].DIR_Name[1] = '.';
    for(size_t i = 2; i < 11; i++){
        dir_table[1].DIR_Name[i] = 0x20;
    }
    dir_table[1].DIR_Attr = FAT32_ATTR_DIRECTORY_BIT;
    dir_table[1].DIR_NTRes = 0;
    dir_table[1].DIR_CrtDate = 0;
    dir_table[1].DIR_CrtTime = 0;
    dir_table[1].DIR_CrtTimeTenth = 0;
    dir_table[1].DIR_WrtTime = 0;
    dir_table[1].DIR_WrtDate = 0;
    dir_table[1].DIR_LstAccDate = 0;
    if(((((uint32_t)parent_entry.entry.DIR_FstClusHI) << 16) | (uint32_t)(parent_entry.entry.DIR_FstClusLO)) == inst->bpb.BPB_RootClus){
        dir_table[1].DIR_FstClusHI = 0ULL; // root is 0 for ..
        dir_table[1].DIR_FstClusLO = 0ULL; // root is 0 for ..
    }else{
        dir_table[1].DIR_FstClusHI = parent_entry.entry.DIR_FstClusHI; // parent
        dir_table[1].DIR_FstClusLO = parent_entry.entry.DIR_FstClusLO; // parent
    }
    dir_table[1].DIR_FileSize = 0;

    if(stor_device->write(inst->data_first_lba + ((dir_clus - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(dir_table)) == FAIL){
        udbP("FAT ERROR: Failed to write directory entries!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        kfree(dir_table);
        return FAIL;
    }
    kfree(dir_table);

    if(uobject_close_object(inst->storage_device) != SUCCESS){
        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        return FAIL;
    }

    // no sfn name for auto random sfn name
    if(fat32_create_dir_entry(inst, parent_entry, dir_name, "", FAT32_ATTR_DIRECTORY_BIT, dir_clus, 0) != SUCCESS){
        udbP("FAT ERROR: Failed to create new directory!");
        return FAIL;
    }

    return SUCCESS;
}

uos_result fat32_get_childs(u_fs_interface* interface, const wide_char_t* dir_path, wide_char_t*** child_names, size_t* names_buffer_size){
    *names_buffer_size = 0;

    fat32_instance* inst = fat32_find_instance(interface->partition);
    if(inst == nullptr){
        udbP("FAT ERROR: File system interface error! H0");
        return FAIL;
    }

    fat32_dir_entry root_dir = {};
    fat32_get_root_dir_virt_entry(inst, &root_dir);

    fat32_entry_result parent_entry = {};
    memcpy(&(parent_entry.entry), &root_dir, sizeof(fat32_dir_entry));

    uint64_t up_dir_count = w_parse_read_count(dir_path, '/');

    wide_char_t read_part[256];
    for(size_t i = 0; i < up_dir_count; i++){
        w_parse_read(read_part, dir_path, '/', i);

        if(read_part[0] == '\0'){
            continue; // empty skip
        }else{
            parent_entry = fat32_find_entry(inst, &(parent_entry.entry), read_part, "");
            if(parent_entry.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE){
                udbP("FAT ERROR: Parent directory not found!");
                return FAIL;
            }
        }
    }

    fat32_dir_entry parent = {};
    memcpy(&parent, &(parent_entry.entry), sizeof(fat32_dir_entry));

    if(!(parent.DIR_Attr & FAT32_ATTR_DIRECTORY_BIT)){
        udbP("FAT ERROR: Parent is not a directory!");
        return FAIL;
    }

    // open device
    udevice_storage_function_pointers* stor_device = _open_storage_device_(inst->storage_device);

    if(stor_device == nullptr){
        udbP("FAT ERROR: Failed to open storage device!");
        return FAIL;
    }

    uint64_t lba_size = stor_device->get_lba_size();
    uint64_t cluster_entry_per_lba = lba_size / sizeof(fat32_cluster_entry);
    uint64_t dir_entry_count_per_cluster = (lba_size * inst->bpb.sector_per_cluster) / sizeof(fat32_dir_entry);

    fat32_cluster_entry* cluster_table = kmalloc(lba_size); // cluster entry table (or FAT)
    fat32_dir_entry* current_table = kmalloc(lba_size * inst->bpb.sector_per_cluster); // cluster
    fat32_long_dir_entry* long_current_table = (fat32_long_dir_entry*)current_table;
    wide_char_t* current_lfn_name = kmalloc(256 * sizeof(wide_char_t)); // + nullterm
    memset(current_lfn_name, 0x0, 256 *  sizeof(wide_char_t));

    fat32_cluster_entry current_cluster = (parent.DIR_FstClusLO | (parent.DIR_FstClusHI << 16));
    size_t long_name_len = 0;
    uint8_t current_lfn_chksum = 0;
    uint8_t expected_lfn_id = 0;

    // dir search
    while(true){
        uint32_t cluster_entry_offset_in_lba = current_cluster % cluster_entry_per_lba;

        // read cluster entry
        // calculate which lba it is in fat & read
        if(stor_device->read(inst->fat1_first_lba + (current_cluster / cluster_entry_per_lba), 1, (uint8_t*)(cluster_table)) == FAIL){
            udbP("FAT ERROR: Failed to read FAT header!");
            if(uobject_close_object(inst->storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            kfree(cluster_table);
            kfree(current_table);
            kfree(current_lfn_name);
            return FAIL;
        }

        // cluster dir search
        // read cluster (directory entries)
        if(stor_device->read(inst->data_first_lba + ((current_cluster - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, (uint8_t*)(current_table)) == FAIL){
            udbP("FAT ERROR: Failed to read directory cluster!");
            if(uobject_close_object(inst->storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            kfree(cluster_table);
            kfree(current_table);
            kfree(current_lfn_name);
            return FAIL;
        }
            
        for(uint32_t i = 0; i < dir_entry_count_per_cluster; i++){
            if(current_table[i].DIR_Name[0] == FAT32_DIR_ENTRY_FREE_START){
                // no any entries available further
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(cluster_table);
                kfree(current_table);
                kfree(current_lfn_name);
                return SUCCESS;
            }else if(current_table[i].DIR_Name[0] != FAT32_DIR_ENTRY_FREE){
                // check long name
                if(current_table[i].DIR_Attr == FAT32_ATTR_LONG_FILE_NAME){
                    if(long_name_len){
                        if(long_current_table[i].LDIR_Ord == expected_lfn_id && long_current_table[i].LDIR_Chksum == current_lfn_chksum){
                            long_name_len = prepend_lfn(&long_current_table[i], current_lfn_name, long_name_len);
                            expected_lfn_id--;
                        }else{
                            // possiable fs corruption
                            udbP("FAT ERROR: LFN mismatch0 found! Maybe filesystem is corrupted?");
                            if(uobject_close_object(inst->storage_device) == FAIL){
                                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                            }
                            kfree(cluster_table);
                            kfree(current_table);
                            kfree(current_lfn_name);
                            return FAIL;
                        }
                    }else{
                        if(long_current_table[i].LDIR_Ord & FAT32_LFN_ID_FLAG_BEGIN){
                            long_name_len = prepend_lfn(&long_current_table[i], current_lfn_name, long_name_len);
                            current_lfn_chksum = long_current_table[i].LDIR_Chksum;
                            expected_lfn_id = (long_current_table[i].LDIR_Ord & ~FAT32_LFN_ID_FLAG_BEGIN) - 1;
                        }else{
                            // possiable fs corruption
                            udbP("FAT ERROR: LFN mismatch1 found! Maybe filesystem is corrupted?");
                            if(uobject_close_object(inst->storage_device) == FAIL){
                                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                            }
                            kfree(cluster_table);
                            kfree(current_table);
                            kfree(current_lfn_name);
                            return FAIL;
                        }
                    }
                }else{
                    if(long_name_len){
                        if(current_lfn_chksum == fat32_calculate_lfn_chksum(current_table[i].DIR_Name) && expected_lfn_id == 0){
                            char* current_name = kmalloc(12 * sizeof(char));
                            memcpy(current_name, current_table[i].DIR_Name, 11);
                            current_name[11] = '\0';

                            (*child_names) = kreloc((*child_names), *names_buffer_size * sizeof(wide_char_t*), ((*names_buffer_size) + 1) * sizeof(wide_char_t*));
                            (*names_buffer_size)++;
                            (*child_names)[(*names_buffer_size) - 1] = kmalloc((wstrlen(current_lfn_name, 257) + 1) * sizeof(wide_char_t));
                            memcpy(((*child_names)[(*names_buffer_size) - 1]), current_lfn_name, (wstrlen(current_lfn_name, 257) + 1) * sizeof(wide_char_t));
                            (*child_names)[(*names_buffer_size) - 1][wstrlen(current_lfn_name, 257)] = '\0';

                            //udbPs();
                            //udbP_wSTR((*child_names)[(*names_buffer_size) - 1]);
                            //udbPe();

                            // go to next entry
                            long_name_len = 0;
                            memset(current_lfn_name, 0x0, 256 *  sizeof(wide_char_t));
                            current_lfn_chksum = 0;
                            expected_lfn_id = 0;
                            kfree(current_name);
                            continue;

                        }else{
                            // possiable fs corruption
                            udbP("FAT ERROR: LFN mismatch2 found! Maybe filesystem is corrupted?");
                            if(uobject_close_object(inst->storage_device) == FAIL){
                                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                            }
                            kfree(cluster_table);
                            kfree(current_table);
                            kfree(current_lfn_name);
                            return FAIL;
                        }
                    }else{
                        char* current_name = kmalloc(12 * sizeof(char));
                        memcpy(current_name, current_table[i].DIR_Name, 11);
                        current_name[11] = '\0';

                        (*child_names) = kreloc((*child_names), *names_buffer_size * sizeof(wide_char_t*), ((*names_buffer_size) + 1) * sizeof(wide_char_t*));
                        (*names_buffer_size)++;
                        (*child_names)[(*names_buffer_size) - 1] = kmalloc((12) * sizeof(wide_char_t));
                        char_to_utf16_buf(current_name, ((*child_names)[(*names_buffer_size) - 1]));

                        //udbPs();
                        //udbP_wSTR((*child_names)[(*names_buffer_size) - 1]);
                        //udbPe();
                        
                        // go to next entry
                        kfree(current_name);
                        continue;
                    }
                }
            }
        }
        
        // not found go to next dir entry cluster
        if(FAT32_CHECK_IS_CLUSTER_ALLOCATED(cluster_table[cluster_entry_offset_in_lba])){
            current_cluster = cluster_table[cluster_entry_offset_in_lba];
        }else{
            break; // no any more cluster left
        }

    }

    if(uobject_close_object(inst->storage_device) == FAIL){
        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
    }
    kfree(cluster_table);
    kfree(current_table);
    kfree(current_lfn_name);
    return SUCCESS;
}

uos_result fat32_delete(u_fs_interface* interface, const wide_char_t* path){
    fat32_instance* inst = fat32_find_instance(interface->partition);
    if(inst == nullptr){
        udbP("FAT ERROR: File system interface error!");
        return FAIL;
    }
    
    fat32_dir_entry root_dir = {};
    fat32_get_root_dir_virt_entry(inst, &root_dir);

    fat32_entry_result deleting_entry = {};
    fat32_entry_result parent_entry = {};
    memcpy(&(parent_entry.entry), &root_dir, sizeof(fat32_dir_entry));
    memcpy(&(deleting_entry.entry), &root_dir, sizeof(fat32_dir_entry));

    uint64_t up_dir_count = w_parse_read_count(path, '/');

    wide_char_t read_part[256];
    for(size_t i = 0; i < up_dir_count; i++){
        w_parse_read(read_part, path, '/', i);

        if(read_part[0] == '\0'){
            continue; // empty skip
        }else{
            parent_entry = deleting_entry;
            deleting_entry = fat32_find_entry(inst, &(deleting_entry.entry), read_part, "");
            if(deleting_entry.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE){
                udbP("FAT ERROR: File not found!");
                return FAIL;
            }
        }
    }

    fat32_dir_entry parent = {};
    memcpy(&parent, &(parent_entry.entry), sizeof(fat32_dir_entry));

    // check is it directory
    if(deleting_entry.entry.DIR_Attr == FAT32_ATTR_DIRECTORY_BIT){
        // check is it empty
        u_fs_file_info del_dir_info = fat32_read_info(interface, path);

        if(del_dir_info.child_count > 2){
            udbP("FAT ERROR: Deleting not empty directory is not allowed! Use manuel solutions.");
            return FAIL;
        }
    }
    
    udevice_storage_function_pointers* stor_device = _open_storage_device_(inst->storage_device);
    if(stor_device == nullptr){
        udbP("FAT ERROR: Failed to open storage device!");
        return FAIL;
    }

    // free clusters if it have
    uint32_t deleting_clus_chain_start = (uint32_t)(deleting_entry.entry.DIR_FstClusLO) | ((uint32_t)deleting_entry.entry.DIR_FstClusHI << 16);
    if(FAT32_CHECK_IS_CLUSTER_ALLOCATED(deleting_clus_chain_start)){
        if(fat32_free_clusters(inst, stor_device, deleting_clus_chain_start, false) != SUCCESS){
            udbP("FAT ERROR: Failed to free clusters of the entry!");
            if(uobject_close_object(inst->storage_device) != SUCCESS){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            return FAIL;
        }
    }
    
    // close device
    if(uobject_close_object(inst->storage_device) == FAIL){
        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        return FAIL;
    }

    if(fat32_delete_entry(inst, &parent, deleting_entry.index_in_dir_list) != SUCCESS){
        udbP("FAT ERROR: Failed to delete entry!");
        return FAIL;
    }
    
    return SUCCESS;
}

uos_result fat32_repair_fs(u_fs_interface* interface){
    udbP("This function is not implemented in driver yet!");
    return FAIL;
}

uos_result fat32_copy(u_fs_interface* interface, const wide_char_t* src_path, const wide_char_t* dst_path){
    fat32_instance* inst = fat32_find_instance(interface->partition);
    if(inst == nullptr){
        udbP("FAT ERROR: File system interface error!");
        return FAIL;
    }

    fat32_dir_entry root_dir = {};
    fat32_get_root_dir_virt_entry(inst, &root_dir);

    // src entry
    fat32_entry_result src_entry = {};
    fat32_entry_result parent_entry = {};
    memcpy(&(parent_entry.entry), &root_dir, sizeof(fat32_dir_entry));
    memcpy(&(src_entry.entry), &root_dir, sizeof(fat32_dir_entry));

    uint64_t src_up_dir_count = w_parse_read_count(src_path, '/');

    wide_char_t src_read_part[256];
    for(size_t i = 0; i < src_up_dir_count; i++){
        w_parse_read(src_read_part, src_path, '/', i);

        if(src_read_part[0] == '\0'){
            continue; // empty skip
        }else{
            parent_entry = src_entry;
            src_entry = fat32_find_entry(inst, &(src_entry.entry), src_read_part, "");
            if(src_entry.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE){
                udbP("FAT ERROR: Source entry not found!");
                return FAIL;
            }
        }
    }
    fat32_dir_entry src_parent = {};
    memcpy(&src_parent, &(parent_entry.entry), sizeof(fat32_dir_entry));

    if(src_entry.entry.DIR_Attr & FAT32_ATTR_DIRECTORY_BIT){
        udbP("FAT ERROR: Copying directories is not implemented! Use manuel solutions.");
        return FAIL;
    }

    // dst dir
    _separated_path sep_dst_path = separate_path(dst_path);
    if(sep_dst_path.file_name == nullptr || sep_dst_path.path == nullptr){
        if(sep_dst_path.file_name != nullptr){
            kfree(sep_dst_path.file_name);
        }
        if(sep_dst_path.path != nullptr){
            kfree(sep_dst_path.path);
        }
        udbP("FAT ERROR: Failed to parse path!");
        return FAIL;
    }

    fat32_entry_result dst_dir_entry = {};
    memcpy(&(parent_entry.entry), &root_dir, sizeof(fat32_dir_entry));
    memcpy(&(dst_dir_entry.entry), &root_dir, sizeof(fat32_dir_entry));

    uint64_t dst_dir_up_dir_count = w_parse_read_count(sep_dst_path.path, '/');

    wide_char_t dst_dir_read_part[256];
    for(size_t i = 0; i < dst_dir_up_dir_count; i++){
        w_parse_read(dst_dir_read_part, sep_dst_path.path, '/', i);

        if(dst_dir_read_part[0] == '\0'){
            continue; // empty skip
        }else{
            parent_entry = dst_dir_entry;
            dst_dir_entry = fat32_find_entry(inst, &(dst_dir_entry.entry), dst_dir_read_part, "");
            if(dst_dir_entry.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE){
                udbP("FAT ERROR: Source entry not found!");
                kfree(sep_dst_path.file_name);
                kfree(sep_dst_path.path);
                return FAIL;
            }
        }
    }
    fat32_dir_entry dst_dir_parent = {};
    memcpy(&dst_dir_parent, &(parent_entry.entry), sizeof(fat32_dir_entry));

    // create copy file
    fat32_create_file(interface,sep_dst_path.path, sep_dst_path.file_name);
    kfree(sep_dst_path.file_name);
    kfree(sep_dst_path.path);

    // write contents to it
    uint8_t* buf = kmalloc(SIZE_1K * 256); // 256kb buffer for copying file
    size_t copied_size = 0;
    while (copied_size < src_entry.entry.DIR_FileSize)
    {
        size_t copy_len = SIZE_1K * 256;
        if((src_entry.entry.DIR_FileSize - copied_size) < copy_len){
            copy_len = src_entry.entry.DIR_FileSize - copied_size;
        }
        if(fat32_read(interface, src_path, copied_size, copy_len, buf) != SUCCESS){
                udbP("FAT ERROR: Failed to copy! H0");
                return FAIL;
        }
        if(fat32_write(interface, src_path, copied_size, copy_len, buf) != SUCCESS){
                udbP("FAT ERROR: Failed to copy! H0");
                return FAIL;
        }
        copied_size += copy_len;
    }

    return SUCCESS;
}

uos_result fat32_move(u_fs_interface* interface, const wide_char_t* src_path, const wide_char_t* dst_path){
    fat32_instance* inst = fat32_find_instance(interface->partition);
    if(inst == nullptr){
        udbP("FAT ERROR: File system interface error!");
        return FAIL;
    }

    fat32_dir_entry root_dir = {};
    fat32_get_root_dir_virt_entry(inst, &root_dir);

    // src entry
    fat32_entry_result src_entry = {};
    fat32_entry_result parent_entry = {};
    memcpy(&(parent_entry.entry), &root_dir, sizeof(fat32_dir_entry));
    memcpy(&(src_entry.entry), &root_dir, sizeof(fat32_dir_entry));

    uint64_t src_up_dir_count = w_parse_read_count(src_path, '/');

    wide_char_t src_read_part[256];
    for(size_t i = 0; i < src_up_dir_count; i++){
        w_parse_read(src_read_part, src_path, '/', i);

        if(src_read_part[0] == '\0'){
            continue; // empty skip
        }else{
            parent_entry = src_entry;
            src_entry = fat32_find_entry(inst, &(src_entry.entry), src_read_part, "");
            if(src_entry.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE){
                udbP("FAT ERROR: Source entry not found!");
                return FAIL;
            }
        }
    }
    fat32_dir_entry src_parent = {};
    memcpy(&src_parent, &(parent_entry.entry), sizeof(fat32_dir_entry));

    if(src_entry.entry.DIR_Attr & FAT32_ATTR_DIRECTORY_BIT){
        udbP("FAT ERROR: Moving directories is not implemented! Use manuel solutions.");
        return FAIL;
    }

    // dst dir
    _separated_path sep_dst_path = separate_path(dst_path);
    if(sep_dst_path.file_name == nullptr || sep_dst_path.path == nullptr){
        if(sep_dst_path.file_name != nullptr){
            kfree(sep_dst_path.file_name);
        }
        if(sep_dst_path.path != nullptr){
            kfree(sep_dst_path.path);
        }
        udbP("FAT ERROR: Failed to parse path!");
        return FAIL;
    }

    fat32_entry_result dst_dir_entry = {};
    memcpy(&(parent_entry.entry), &root_dir, sizeof(fat32_dir_entry));
    memcpy(&(dst_dir_entry.entry), &root_dir, sizeof(fat32_dir_entry));

    uint64_t dst_dir_up_dir_count = w_parse_read_count(sep_dst_path.path, '/');

    wide_char_t dst_dir_read_part[256];
    for(size_t i = 0; i < dst_dir_up_dir_count; i++){
        w_parse_read(dst_dir_read_part, sep_dst_path.path, '/', i);

        if(dst_dir_read_part[0] == '\0'){
            continue; // empty skip
        }else{
            parent_entry = dst_dir_entry;
            dst_dir_entry = fat32_find_entry(inst, &(dst_dir_entry.entry), dst_dir_read_part, "");
            if(dst_dir_entry.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE){
                udbP("FAT ERROR: Source entry not found!");
                kfree(sep_dst_path.file_name);
                kfree(sep_dst_path.path);
                return FAIL;
            }
        }
    }
    fat32_dir_entry dst_dir_parent = {};
    memcpy(&dst_dir_parent, &(parent_entry.entry), sizeof(fat32_dir_entry));

    uint32_t cluster = ((uint32_t)src_entry.entry.DIR_FstClusLO & 0xFFFF) || (((uint32_t)src_entry.entry.DIR_FstClusHI >> 16) & 0xFFFF);
    if(fat32_create_dir_entry(inst, parent_entry, sep_dst_path.file_name, "", src_entry.entry.DIR_Attr, cluster, src_entry.entry.DIR_FileSize) != SUCCESS){
        udbP("FAT ERROR: Failed to move! H0");
        kfree(sep_dst_path.file_name);
        kfree(sep_dst_path.path);
        return FAIL;
    }

    if(fat32_delete_entry(inst, &src_parent, src_entry.index_in_dir_list) != SUCCESS){
        udbP("FAT ERROR: Failed to move! H1");
        kfree(sep_dst_path.file_name);
        kfree(sep_dst_path.path);
        return FAIL;
    }

    kfree(sep_dst_path.file_name);
    kfree(sep_dst_path.path);
    return SUCCESS;
}

uos_result fat32_resize_file(u_fs_interface* interface, const wide_char_t* file_path, size_t new_size){
    if(new_size > (SIZE_1G * 4)){
        udbP("FAT ERROR: A file can not be larger than 4gb!");
        return FAIL;
    }

    fat32_instance* inst = fat32_find_instance(interface->partition);
    if(inst == nullptr){
        udbP("FAT ERROR: File system interface error!");
        return FAIL;
    }

    // find entry
    fat32_dir_entry root_dir = {};
    fat32_get_root_dir_virt_entry(inst, &root_dir);

    fat32_entry_result file_entry = {};
    memcpy(&(file_entry.entry), &root_dir, sizeof(fat32_dir_entry));

    uint64_t up_dir_count = w_parse_read_count(file_path, '/');

    wide_char_t read_part[256];
    for(size_t i = 0; i < up_dir_count; i++){
        w_parse_read(read_part, file_path, '/', i);

        if(read_part[0] == '\0'){
            continue; // empty skip
        }else{
            file_entry = fat32_find_entry(inst, &(file_entry.entry), read_part, "");
            if(file_entry.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE){
                udbP("FAT ERROR: File not found!");
                return FAIL;
            }
        }
    }

    fat32_dir_entry parent = {};
    memcpy(&parent, &(file_entry.entry), sizeof(fat32_dir_entry));

    if(file_entry.entry.DIR_Attr & FAT32_ATTR_DIRECTORY_BIT){
        udbP("FAT ERROR: Can not resize a directory!");
        return FAIL;
    }

    if(file_entry.entry.DIR_FileSize == new_size){
        return SUCCESS; // no resize required
    }

    uint32_t file_data_cluster_chain_start = (uint32_t)(file_entry.entry.DIR_FstClusLO) | ((uint32_t)file_entry.entry.DIR_FstClusHI << 16);

    udevice_storage_function_pointers* stor_device =  _open_storage_device_(inst->storage_device);
    if(stor_device == nullptr){
        udbP("FAT STORAGE DEVICE ERROR: Can not open device!");
        return FAIL;
    }
    fat32_cluster_chain ch = fat32_get_chain(inst, stor_device, file_data_cluster_chain_start);
    size_t cluster_size = inst->bpb.sector_per_cluster * stor_device->get_lba_size();
    size_t required_cluster_count = (new_size + cluster_size - 1) / cluster_size;

    if(required_cluster_count == ch.chain_len){
        // no cluster allocation needed
        // just update file size & return
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            return FAIL;
        }
        if(ch.chain != nullptr){
            kfree(ch.chain);
        }
        file_entry.entry.DIR_FileSize = new_size;
        if(fat32_update_entry(inst, &parent, file_entry.index_in_dir_list, &(file_entry.entry)) != SUCCESS){
            udbP("FAT ERROR: Failed to update file entry! H0");
            return FAIL;
        }
        return SUCCESS;
    }

    if(ch.chain_len < required_cluster_count){
        // expand file
        if(ch.chain != nullptr){
            // chain exists
            if(fat32_allocate_clusters(inst, stor_device, ch.chain[ch.chain_len - 1], nullptr, required_cluster_count - ch.chain_len, true) != SUCCESS){
                udbP("FAT ERROR: Failed to expand file! H0");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(ch.chain);
                return FAIL;
            }
        }else{
            // chain doesnt exists
            if(fat32_allocate_clusters(inst, stor_device, FAT32_CLUSTER_FREE, &file_data_cluster_chain_start, required_cluster_count, true) != SUCCESS){
                udbP("FAT ERROR: Failed to expand file! H1");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(ch.chain);
                return FAIL;
            }
        }
        // expansion done
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            return FAIL;
        }
        kfree(ch.chain);
        file_entry.entry.DIR_FileSize = new_size;
        file_entry.entry.DIR_FstClusLO = (file_data_cluster_chain_start & 0xFFFF);
        file_entry.entry.DIR_FstClusHI = ((file_data_cluster_chain_start >> 16) & 0xFFFF);
        if(fat32_update_entry(inst, &parent, file_entry.index_in_dir_list, &(file_entry.entry)) != SUCCESS){
            udbP("FAT ERROR: Failed to update file entry! H1");
            return FAIL;
        }
        return SUCCESS;
    }else{
        // shrink file
        if(required_cluster_count == 0){
            // free all
            if(uobject_close_object(inst->storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            kfree(ch.chain);
            file_entry.entry.DIR_FileSize = new_size;
            file_entry.entry.DIR_FstClusLO = 0;
            file_entry.entry.DIR_FstClusHI = 0;
            if(fat32_update_entry(inst, &parent, file_entry.index_in_dir_list, &(file_entry.entry)) != SUCCESS){
                udbP("FAT ERROR: Failed to update file entry! H2");
                return FAIL;
            }
            return SUCCESS;
        }

        if(ch.chain != nullptr){
            // chain exists
            if(fat32_free_clusters(inst, stor_device, ch.chain[required_cluster_count - 1], true) != SUCCESS){
                udbP("FAT ERROR: Failed to shrink file! H0");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(ch.chain);
                return FAIL;
            }
        }else{
            // chain doesnt exists
            udbP("FAT ERROR: Failed to shrink file! H1");
            if(uobject_close_object(inst->storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            kfree(ch.chain);
            return FAIL;
        }
        // shrinking done
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            return FAIL;
        }
        kfree(ch.chain);
        file_entry.entry.DIR_FileSize = new_size;
        file_entry.entry.DIR_FstClusLO = (file_data_cluster_chain_start & 0xFFFF);
        file_entry.entry.DIR_FstClusHI = ((file_data_cluster_chain_start >> 16) & 0xFFFF);
        if(fat32_update_entry(inst, &parent, file_entry.index_in_dir_list, &(file_entry.entry)) != SUCCESS){
            udbP("FAT ERROR: Failed to update file entry! H3");
            return FAIL;
        }
        return SUCCESS;
    }
}

uos_result fat32_read(u_fs_interface* interface, const wide_char_t* file_path, size_t read_offset, size_t read_len, void* read_buffer){
    fat32_instance* inst = fat32_find_instance(interface->partition);
    if(inst == nullptr){
        udbP("FAT ERROR: File system interface error!");
        return FAIL;
    }

    // find entry
    fat32_dir_entry root_dir = {};
    fat32_get_root_dir_virt_entry(inst, &root_dir);

    fat32_entry_result file_entry = {};
    memcpy(&(file_entry.entry), &root_dir, sizeof(fat32_dir_entry));

    uint64_t up_dir_count = w_parse_read_count(file_path, '/');

    wide_char_t read_part[256];
    for(size_t i = 0; i < up_dir_count; i++){
        w_parse_read(read_part, file_path, '/', i);

        if(read_part[0] == '\0'){
            continue; // empty skip
        }else{
            file_entry = fat32_find_entry(inst, &(file_entry.entry), read_part, "");
            if(file_entry.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE){
                udbP("FAT ERROR: File not found!");
                return FAIL;
            }
        }
    }

    fat32_dir_entry parent = {};
    memcpy(&parent, &(file_entry.entry), sizeof(fat32_dir_entry));

    if(file_entry.entry.DIR_Attr & FAT32_ATTR_DIRECTORY_BIT){
        udbP("FAT ERROR: Can not read from a directory!");
        return FAIL;
    }

    if((read_offset > file_entry.entry.DIR_FileSize) || (read_len > (file_entry.entry.DIR_FileSize - read_offset))){
        udbP("FAT ERROR: Requested read area is out of bound!");
        return FAIL;
    }

    if(read_len == 0){
        return SUCCESS;
    }

    udevice_storage_function_pointers* stor_device =  _open_storage_device_(inst->storage_device);
    if(stor_device == nullptr){
        udbP("FAT STORAGE DEVICE ERROR: Can not open device!");
        return FAIL;
    }

    uint32_t file_data_cluster_chain_start = (uint32_t)(file_entry.entry.DIR_FstClusLO) | ((uint32_t)file_entry.entry.DIR_FstClusHI << 16);
    if(!FAT32_CHECK_IS_CLUSTER_ALLOCATED(file_data_cluster_chain_start)){
        udbP("FAT ERROR: File have corrupted or invalid data clusters! Maybe file system is corrupted?");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    fat32_cluster_chain ch = fat32_get_chain(inst, stor_device, file_data_cluster_chain_start);
    if(ch.chain == nullptr){
        udbP("FAT ERROR: Failed to get cluster chain for file data!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    uint32_t currently_read_cluster_idx = read_offset / (inst->bpb.sector_per_cluster * stor_device->get_lba_size());
    if(ch.chain_len <= currently_read_cluster_idx){
        udbP("FAT ERROR: Reading cluster is not in bound! Maybe file system is corrupted?");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        kfree(ch.chain);
        return FAIL;
    }

    size_t bytes_read = 0;

    uint8_t* current_cluster = kmalloc(stor_device->get_lba_size() * inst->bpb.sector_per_cluster); // cluster

    // read
    if(stor_device->read(inst->data_first_lba + ((ch.chain[currently_read_cluster_idx] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, current_cluster) == FAIL){
        udbP("FAT ERROR: Failed to read directory cluster!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        kfree(current_cluster);
        kfree(ch.chain);
        return FAIL;
    }

    while (bytes_read != read_len)
    {
        size_t current_offset = read_offset + bytes_read;
        uint32_t required_cluster_idx = current_offset / (inst->bpb.sector_per_cluster * stor_device->get_lba_size());
        if(required_cluster_idx != currently_read_cluster_idx){
            // read required
            // no need to write back
            currently_read_cluster_idx = required_cluster_idx;
            if(ch.chain_len <= currently_read_cluster_idx){
                udbP("FAT ERROR: Reading cluster is not in bound! Maybe file system is corrupted?");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(current_cluster);
                kfree(ch.chain);
                return FAIL;
            }

            if(stor_device->read(inst->data_first_lba + ((ch.chain[currently_read_cluster_idx] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, current_cluster) == FAIL){
                udbP("FAT ERROR: Failed to read data cluster! D0");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(current_cluster);
                kfree(ch.chain);
                return FAIL;
            }
        }

        size_t in_cluster_offset = current_offset % (inst->bpb.sector_per_cluster * stor_device->get_lba_size());
        size_t read_size = (inst->bpb.sector_per_cluster * stor_device->get_lba_size()) - in_cluster_offset;
        if((read_len - bytes_read) < read_size){
            read_size = (read_len - bytes_read);
        }
        memcpy((uint8_t*)(read_buffer) + bytes_read, current_cluster + in_cluster_offset, read_size);
        bytes_read += read_size;
    }
    
    kfree(current_cluster);
    kfree(ch.chain);
    if(uobject_close_object(inst->storage_device) == FAIL){
        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        return FAIL;
    }
    return SUCCESS;
}

uos_result fat32_write(u_fs_interface* interface, const wide_char_t* file_path, size_t write_offset, size_t write_len, void* write_buffer){ 
    fat32_instance* inst = fat32_find_instance(interface->partition);
    if(inst == nullptr){
        udbP("FAT ERROR: File system interface error!");
        return FAIL;
    }

    u_fs_file_info info = fat32_read_info(interface, file_path);

    if(!(info.flags & FSI_FLAG_DIRECTORY_BIT)){
        if(info.flags & FSI_FLAG_RO_BIT){
            udbP("FAT ERROR: File is read only!");
            return FAIL;
        }

        if((write_offset > info.file_size) || (write_len > (info.file_size - write_offset))){
            // resize file
            if(resize_file(file_path, (write_offset + write_len)) != SUCCESS){
                udbP("FAT ERROR: Can not resize the file!");
                return FAIL;
            }
        }
    }else{
        udbP("FAT ERROR: Can not write to a directory!");
        return FAIL;
    }

    // find entry
    fat32_dir_entry root_dir = {};
    fat32_get_root_dir_virt_entry(inst, &root_dir);

    fat32_entry_result file_entry = {};
    memcpy(&(file_entry.entry), &root_dir, sizeof(fat32_dir_entry));

    uint64_t up_dir_count = w_parse_read_count(file_path, '/');

    wide_char_t read_part[256];
    for(size_t i = 0; i < up_dir_count; i++){
        w_parse_read(read_part, file_path, '/', i);

        if(read_part[0] == '\0'){
            continue; // empty skip
        }else{
            file_entry = fat32_find_entry(inst, &(file_entry.entry), read_part, "");
            if(file_entry.entry.DIR_Name[0] == FAT32_DIR_ENTRY_FREE){
                udbP("FAT ERROR: File not found!");
                return FAIL;
            }
        }
    }

    fat32_dir_entry parent = {};
    memcpy(&parent, &(file_entry.entry), sizeof(fat32_dir_entry));

    if(file_entry.entry.DIR_Attr & FAT32_ATTR_DIRECTORY_BIT){
        udbP("FAT ERROR: Can not read from a directory!");
        return FAIL;
    }

    if(write_len == 0){
        return SUCCESS;
    }

    udevice_storage_function_pointers* stor_device =  _open_storage_device_(inst->storage_device);
    if(stor_device == nullptr){
        udbP("FAT STORAGE DEVICE ERROR: Can not open device!");
        return FAIL;
    }

    uint32_t file_data_cluster_chain_start = (uint32_t)(file_entry.entry.DIR_FstClusLO) | ((uint32_t)file_entry.entry.DIR_FstClusHI << 16);
    if(!FAT32_CHECK_IS_CLUSTER_ALLOCATED(file_data_cluster_chain_start)){
        udbP("FAT ERROR: File have corrupted or invalid data clusters! Maybe file system is corrupted?");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    fat32_cluster_chain ch = fat32_get_chain(inst, stor_device, file_data_cluster_chain_start);
    if(ch.chain == nullptr){
        udbP("FAT ERROR: Failed to get cluster chain for file data!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    uint32_t currently_read_cluster_idx = write_offset / (inst->bpb.sector_per_cluster * stor_device->get_lba_size());
    if(ch.chain_len <= currently_read_cluster_idx){
        udbP("FAT ERROR: Writing cluster is not in bound! Maybe file system is corrupted?");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        kfree(ch.chain);
        return FAIL;
    }

    size_t bytes_wrote = 0;

    uint8_t* current_cluster = kmalloc(stor_device->get_lba_size() * inst->bpb.sector_per_cluster); // cluster

    // read
    if(stor_device->read(inst->data_first_lba + ((ch.chain[currently_read_cluster_idx] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, current_cluster) == FAIL){
        udbP("FAT ERROR: Failed to read data cluster!");
        if(uobject_close_object(inst->storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        kfree(current_cluster);
        kfree(ch.chain);
        return FAIL;
    }

    while (bytes_wrote != write_len)
    {
        size_t current_offset = write_offset + bytes_wrote;
        uint32_t required_cluster_idx = current_offset / (inst->bpb.sector_per_cluster * stor_device->get_lba_size());

        size_t cluster_size = inst->bpb.sector_per_cluster * stor_device->get_lba_size();
        size_t in_cluster_offset = current_offset % cluster_size;
        size_t write_size = cluster_size - in_cluster_offset;
        size_t remaining = write_len - bytes_wrote;

        if (write_size > remaining) {
            write_size = remaining;
        }

        if((required_cluster_idx != currently_read_cluster_idx ) && ((in_cluster_offset != 0) || (write_size < ((inst->bpb.sector_per_cluster * stor_device->get_lba_size()) - in_cluster_offset)))){
            // read if writing part is not bounding all of cluster
            currently_read_cluster_idx = required_cluster_idx;
            if(ch.chain_len <= currently_read_cluster_idx){
                udbP("FAT ERROR: Writing cluster is not in bound! Maybe file system is corrupted?");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(current_cluster);
                kfree(ch.chain);
                return FAIL;
            }

            if(stor_device->read(inst->data_first_lba + ((ch.chain[currently_read_cluster_idx] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, current_cluster) == FAIL){
                udbP("FAT ERROR: Failed to read data cluster! D0");
                if(uobject_close_object(inst->storage_device) == FAIL){
                    udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
                }
                kfree(current_cluster);
                kfree(ch.chain);
                return FAIL;
            }
        }
        currently_read_cluster_idx = required_cluster_idx;

        memcpy(current_cluster + in_cluster_offset, (uint8_t*)(write_buffer) + bytes_wrote, write_size);
        bytes_wrote += write_size;

        // write
        if(stor_device->write(inst->data_first_lba + ((ch.chain[currently_read_cluster_idx] - 2) * inst->bpb.sector_per_cluster), inst->bpb.sector_per_cluster, current_cluster) == FAIL){
            udbP("FAT ERROR: Failed to write data cluster! D0");
            if(uobject_close_object(inst->storage_device) == FAIL){
                udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
            }
            kfree(current_cluster);
            kfree(ch.chain);
            return FAIL;
        }
    }
    
    kfree(current_cluster);
    kfree(ch.chain);
    if(uobject_close_object(inst->storage_device) == FAIL){
        udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        return FAIL;
    }
    return SUCCESS;
}

uos_result mount_fat32_partition(partition_info partition, uobject_ref storage_device){
    udevice_storage_function_pointers* stor_device = _open_storage_device_(storage_device);

    if(stor_device == nullptr){
        return FAIL;
    }

    if(stor_device->get_lba_size() != 512){
        udbP("FAT STORAGE DEVICE ERROR: THis driver is not safe for sector size smaller than 512!");
        if(uobject_close_object(storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    fat32_instance inst;

    inst.storage_device = storage_device;
    inst.partition = partition;

    if(stor_device->read(partition.start_lba + 0, 1, (uint8_t*)(&inst.bpb)) == FAIL){
        udbP("FAT ERROR: Failed to read FAT header!");
        if(uobject_close_object(storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    if(stor_device->read(partition.start_lba + inst.bpb.BPB_FSInfo, 1, (uint8_t*)(&inst.FSInfo)) == FAIL){
        udbP("FAT ERROR: Failed to read FSInfo header!");
        if(uobject_close_object(storage_device) == FAIL){
            udbP("FAT STORAGE DEVICE ERROR: Failed to close storage device!");
        }
        return FAIL;
    }

    inst.fat1_first_lba = partition.start_lba + inst.bpb.reserved_sector_count;
    inst.fat2_first_lba = inst.fat1_first_lba + inst.bpb.BPB_FATSz32;
    inst.data_first_lba = inst.fat2_first_lba + inst.bpb.BPB_FATSz32;
    inst.data_cluster_count = (inst.bpb.TotSec32 - inst.bpb.reserved_sector_count - (inst.bpb.BPB_FATSz32 * inst.bpb.number_of_fats)) / inst.bpb.sector_per_cluster;

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
    fat32_fsi.create_dir = fat32_create_dir;
    fat32_fsi.delete = fat32_delete;
    fat32_fsi.read_info = fat32_read_info;
    fat32_fsi.read = fat32_read;
    fat32_fsi.write = fat32_write;
    fat32_fsi.repair_fs = fat32_repair_fs;
    fat32_fsi.copy = fat32_copy;
    fat32_fsi.move = fat32_move;
    fat32_fsi.resize_file = fat32_resize_file;
    fat32_fsi.get_childs = fat32_get_childs;

    uobject_ref obj = uobject_create_fsi(partition.name, 0, fat32_fsi);
    if(obj == UOBJECT_NOT_DEFINED){
        udbP("FAT UOBJECT ERROR: Failed to create filesystem interface UOBJECT!");
        return FAIL;
    }

    return vfs_create_object_file("/mounts/", partition.name, obj, VFS_TYPE_FS_PARTITION, 0);
}
