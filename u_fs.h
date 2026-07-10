/*
This file system was only works with 512byte or higher block sizes
*/

#ifndef U_FS_H
#define U_FS_H

#include <memory/u_memory.h>

#include "u_spi_flash.h"

// debug
#include "u_uart.h"
#include "u_terminal.h"
#include "u_cstr_util.h"

#define UFS_SUCCESS 0
#define UFS_FAIL 1

#define UFS_VER_STR "UFS002"

int ufs_init_sd();

int ufs_format_sd();

#endif