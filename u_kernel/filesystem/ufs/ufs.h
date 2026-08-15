#ifndef UFS_H
#define UFS_H

#include <u_kernel/util/u_ctypes.h>

#define UFS_FAIL -1
#define UFS_SUCCESS 0

#define UFS_GPT_GUID_LOW  (0xFADC3234522EE432)
#define UFS_GPT_GUID_HIGH (0xCFAD0321456EE432)

int format_sd_gpt_with_pre_partitions();

#endif