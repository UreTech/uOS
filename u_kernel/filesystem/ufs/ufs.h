#ifndef UFS_H
#define UFS_H

#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/objects/uobject.h>
typedef uint64_t uobject_ref;

#define UFS_FAIL FAIL
#define UFS_SUCCESS SUCCESS

#define UFS_GPT_GUID_LOW  (0xFADC3234522EE432)
#define UFS_GPT_GUID_HIGH (0xCFAD0321456EE432)

#define FS_GUID_INVALID (0ULL)
#define FS_GUID_UNKNOWN (1ULL)
#define FS_GUID_UFS (2ULL)
#define FS_GUID_FAT (3ULL)

uos_result format_sd_gpt_with_pre_partitions(uobject_ref device_ref);

uos_result get_partition_from_device(uobject_ref device_ref, partition_info* partitions, size_t max_entry_count_to_read);

uos_result try_mount_partition(uobject_ref device_ref, partition_info partition);

uint64_t mount_all_partitions(uobject_ref device_ref);

#endif