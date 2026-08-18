#ifndef U_DEVICE_H
#define U_DEVICE_H

#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/drivers/emmc/u_emmc.h>

// path
#define UDEVICE_PATH_END (0ULL)
#define UDEVICE_PATH_AXIBUS (0xA0ULL)

#define UDEVICE_PATH_PCIE (0xA1ULL)
#define UDEVICE_PATH_VL805 (0xB1ULL)

#define UDEVICE_PATH_GETH (0xA2ULL) // gigabit ethernet

#define UDEVICE_PATH_ARASAN_SDIO (0xA3ULL)
#define UDEVICE_PATH_WLAN (0xC1ULL)
#define UDEVICE_PATH_EMMC (0xC2ULL)

#define UDEVICE_PATH_UART (0xA4ULL)
#define UDEVICE_PATH_BLUETOOTH (0xD1ULL)

#define UDEVICE_PATH_VC6_V3D (0xA5ULL) // gpu
#define UDEVICE_PATH_DRAM (0xE1ULL)

#define UDEVICE_PATH_DMA (0xA6ULL)

#define UDEVICE_PATH_ARM (0xF0ULL) // cpu

// family
#define UDEVICE_FAMILY_NULL (0ULL)
#define UDEVICE_FAMILY_STORAGE (1ULL)
#define UDEVICE_FAMILY_USB (2ULL)
#define UDEVICE_FAMILY_RF (3ULL)
#define UDEVICE_FAMILY_ETH (4ULL)
#define UDEVICE_FAMILY_PU (5ULL)
#define UDEVICE_FAMILY_MEMORY (5ULL)

// type
#define UDEVICE_TYPE_NULL_STORAGE (0ULL)

#define UDEVICE_TYPE_EMMC_STORAGE (1ULL)

#define UDEVICE_TYPE_USB_STORAGE (2ULL)

#define UDEVICE_TYPE_GPIO_STORAGE (3ULL)

#define UDEVICE_TYPE_WLAN (4ULL)
#define UDEVICE_TYPE_RFCOM (5ULL)

#define UDEVICE_TYPE_GETH (6ULL) // gigabit ethernet

#define UDEVICE_TYPE_CPU (7ULL) // cpu
#define UDEVICE_TYPE_GPU (8ULL) // gpu

#define UDEVICE_TYPE_DMA (9ULL)
#define UDEVICE_TYPE_DRAM (10ULL)

#define UDEVICE_TYPE_CUSTOM (11ULL)

typedef struct{
    uint64_t family; // family id
    uint64_t type; // type id
    uint64_t symbols[12]; // func / var ptr etc.
    uint8_t path[8]; // path
}__attribute__((aligned(16))) udevice;

// common storage symbol idx
#define UDEV_SYM_STORAGE_GET_LBA_COUNT_IDX (0ULL)
#define UDEV_SYM_STORAGE_READ_IDX (1ULL)
#define UDEV_SYM_STORAGE_WRITE_IDX (2ULL)
#define UDEV_SYM_STORAGE_SAFE_SHUTDOWN_IDX (3ULL)
#define UDEV_SYM_STORAGE_SOFTWARE_RESET_IDX (4ULL)
#define UDEV_SYM_STORAGE_START_IDX (5ULL)
// emmc sd card
#define UDEV_SYM_EMMC_STORAGE_CARD_INFO_IDX (6ULL)
// usb
#define UDEV_SYM_USB_STORAGE_INFO_IDX (6ULL)
// gpio
#define UDEV_SYM_GPIO_STORAGE_INFO_IDX (6ULL)

// common storage function pointers
typedef size_t (*udev_storage_get_lba_count_fptr)(); // idx 0
typedef uos_result (*udev_storage_read_fptr)(uint64_t start_lba, size_t lba_count, uint8_t* read_buffer); // idx 1
typedef uos_result (*udev_storage_write_fptr)(uint64_t start_lba, size_t lba_count, uint8_t* write_buffer); // idx 2
typedef uos_result (*udev_storage_safe_shutdown_fptr)(); // idx 3
typedef uos_result (*udev_storage_software_reset_fptr)(); // idx 4
typedef uos_result (*udev_storage_start_fptr)(); // idx 5
// emmc sd card
typedef _sd_card_info_ (*udev_emmc_storage_card_info_fptr)(); // idx 6

typedef struct
{
    udev_storage_get_lba_count_fptr get_lba_count; // idx 0
    udev_storage_read_fptr read; // idx 1
    udev_storage_write_fptr write; // idx 2
    udev_storage_safe_shutdown_fptr safe_shutdown; // idx 3
    udev_storage_software_reset_fptr software_reset; // idx 4
    udev_storage_start_fptr start; // idx 5
}__attribute__((aligned(16))) udevice_storage_function_pointers;

typedef struct
{
    udevice_storage_function_pointers common; // idx 0-5
    udev_emmc_storage_card_info_fptr get_card_information; // idx 6
}__attribute__((aligned(16))) udevice_emmc_storage_function_pointers;

#endif