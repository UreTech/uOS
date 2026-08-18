#ifndef UFS_H
#define UFS_H

#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/objects/uobject.h>
#define UFS_FAIL FAIL
#define UFS_SUCCESS SUCCESS

#define UFS_GPT_GUID_LOW  (0xFADC3234522EE432)
#define UFS_GPT_GUID_HIGH (0xCFAD0321456EE432)

#define FS_GUID_INVALID (0ULL)
#define FS_GUID_UNKNOWN (1ULL)
#define FS_GUID_UFS (2ULL)
#define FS_GUID_FAT (3ULL)

uos_result format_sd_gpt_with_pre_partitions(uobject_ref device_ref);

typedef struct
{
    uint64_t start_lba;
    size_t lba_count;
    char name[48];
}partition_info;

uos_result get_partition_from_device(uobject_ref device_ref, partition_info* partitions, size_t max_entry_count_to_read);

udevice_emmc_storage_function_pointers* _open_emmc_storage_device_(uobject_ref device_ref);


#endif