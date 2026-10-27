#ifndef FSI_H
#define FSI_H

#include <u_kernel/util/u_ctypes.h>

typedef struct
{
    uint64_t start_lba;
    size_t lba_count;
    char name[48];
}__attribute__((aligned(16))) partition_info;

#define UOS_FS_TYPE_UNKNOWN (0ULL)
#define UOS_FS_TYPE_UFS (1ULL)
#define UOS_FS_TYPE_FAT (2ULL)
#define UOS_FS_TYPE_CUSTOM (3ULL)

#define FSI_FLAG_INVALID_BIT ONEBIT(0)
#define FSI_FLAG_DIRECTORY_BIT ONEBIT(1)
#define FSI_FLAG_RO_BIT ONEBIT(2)
#define FSI_FLAG_EXECUTE_BIT ONEBIT(3)
#define FSI_FLAG_HIDDEN_BIT ONEBIT(4)
#define FSI_FLAG_FLAG2_EXTENSION_SUPPORTED_BIT ONEBIT(63) // not fully implemented yet (recommended to not using it)
typedef uint64_t fsi_flags;
typedef uint64_t uobject_ref;

typedef struct{
    fsi_flags flags;
    size_t child_count; // only dir
    size_t file_size; // not strictly implemented for directories (maybe return 0 or child_count for dirs)
}__attribute__((aligned(16))) u_fs_file_info;

typedef struct u_fs_interface u_fs_interface;

typedef uos_result (*u_fs_interface_create_file_fptr)(u_fs_interface* interface, const wide_char_t* parent, const wide_char_t* file_name);
typedef uos_result (*u_fs_interface_create_dir_fptr)(u_fs_interface* interface, const wide_char_t* parent, const wide_char_t* dir_name);
typedef uos_result (*u_fs_interface_delete_fptr)(u_fs_interface* interface, const wide_char_t* path);
typedef u_fs_file_info (*u_fs_interface_read_info_fptr)(u_fs_interface* interface, const wide_char_t* path);
typedef uos_result (*u_fs_interface_read_fptr)(u_fs_interface* interface, const wide_char_t* file_path, size_t read_offset, size_t read_len, void* read_buffer);
typedef uos_result (*u_fs_interface_write_fptr)(u_fs_interface* interface, const wide_char_t* file_path, size_t write_offset, size_t write_len, void* write_buffer);
typedef uos_result (*u_fs_interface_repair_fs_fptr)(u_fs_interface* interface);
typedef uos_result (*u_fs_interface_copy_fptr)(u_fs_interface* interface, const wide_char_t* src_path, const wide_char_t* dst_path);
typedef uos_result (*u_fs_interface_move_fptr)(u_fs_interface* interface, const wide_char_t* src_path, const wide_char_t* dst_path);
typedef uos_result (*u_fs_interface_resize_file_fptr)(u_fs_interface* interface, const wide_char_t* file_path, size_t new_size);
typedef uos_result (*u_fs_interface_get_childs_fptr)(u_fs_interface* interface, const wide_char_t* dir_path, wide_char_t*** child_names, size_t* names_buffer_size); // NOTE: DO NOT FORGET TO FREE BUFFERS AFTER USE!

struct u_fs_interface{
    char fs_type_name[32];
    uobject_ref storage_device;
    uint8_t padding[8]; 
    partition_info partition;
    u_fs_interface_create_file_fptr create;
    u_fs_interface_create_dir_fptr create_dir;
    u_fs_interface_delete_fptr delete;
    u_fs_interface_read_info_fptr read_info;
    u_fs_interface_read_fptr read;
    u_fs_interface_write_fptr write;
    u_fs_interface_repair_fs_fptr repair_fs;
    u_fs_interface_copy_fptr copy;
    u_fs_interface_move_fptr move;
    u_fs_interface_resize_file_fptr resize_file;
    u_fs_interface_get_childs_fptr get_childs; // NOTE: DO NOT FORGET TO FREE BUFFERS AFTER USE!

}__attribute__((aligned(16)));

#endif