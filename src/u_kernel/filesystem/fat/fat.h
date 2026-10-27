#ifndef FAT_H
#define FAT_H

#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/filesystem/ufs/ufs.h>
#include <u_kernel/objects/uobject.h>

// returns FAIL if head is not found
// returns SUCCESS if head found
// also repairs head if any repairable corruption detected
uos_result check_fat32_partition(partition_info partition, uobject_ref storage_device);

uos_result create_fat32_partition(partition_info partition, uobject_ref storage_device);

uos_result mount_fat32_partition(partition_info partition, uobject_ref storage_device);

#endif