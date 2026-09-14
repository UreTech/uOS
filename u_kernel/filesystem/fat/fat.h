#ifndef FAT_H
#define FAT_H

#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/filesystem/ufs/ufs.h>
#include <u_kernel/objects/uobject.h>

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

// returns FAIL if head is not found
// returns SUCCESS if head found
// also repairs head if any repairable corruption detected
uos_result check_fat32_partition(partition_info partition, uobject_ref storage_device);

// calculates BPB_FATSz32
uint64_t fatgen103(size_t partition_lba_count, uint16_t reserved_sector_count, uint8_t sector_per_cluster, uint8_t num_of_fats);


#endif