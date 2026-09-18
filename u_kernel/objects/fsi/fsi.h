#ifndef FSI_H
#define FSI_H

#include <u_kernel/objects/uobject.h>

#define UOS_FS_TYPE_UNKNOWN (0ULL)
#define UOS_FS_TYPE_UFS (1ULL)
#define UOS_FS_TYPE_FAT (2ULL)

#define FSI_FLAG_DIRECTORY_BIT ONEBIT(0)
typedef uint64_t fsi_flags;

typedef struct{
    fsi_flags flags;
    size_t child_count; // only dir
    size_t file_size; // not strictly implemented for directories (maybe return 0 for dirs)
}__attribute__((aligned(16))) u_fs_file_info;

typedef uos_result (*u_fs_interface_create_file_fptr)(u_fs_interface* interface, const char* parent, const char* file_name);
typedef uos_result (*u_fs_interface_create_dir_fptr)(u_fs_interface* interface, const char* parent, const char* dir_name);
typedef uos_result (*u_fs_interface_delete_file_fptr)(u_fs_interface* interface, const char* file_path);
typedef u_fs_file_info (*u_fs_interface_read_file_info_fptr)(u_fs_interface* interface, const char* file_path);
typedef uos_result (*u_fs_interface_read_fptr)(u_fs_interface* interface, const char* file_path, size_t read_offset, size_t read_len, void* read_buffer);
typedef uos_result (*u_fs_interface_write_fptr)(u_fs_interface* interface, const char* file_path, size_t write_offset, size_t write_len, void* write_buffer);
typedef uos_result (*u_fs_interface_repair_fs_fptr)(u_fs_interface* interface);
typedef uos_result (*u_fs_interface_rename_file_fptr)(u_fs_interface* interface, const char* file_path, const char* new_name);
typedef uos_result (*u_fs_interface_get_childs_fptr)(u_fs_interface* interface, const char* dir_path, const char** child_names, size_t names_buffer_size); // NOTE: DO NOT FORGET TO FREE BUFFERS AFTER USE!

typedef struct{
    char fs_type_name[48];
    uobject_ref storage_device;
    partition_info partition;
    u_fs_interface_create_file_fptr create;
    u_fs_interface_create_dir_fptr create_dir;
    u_fs_interface_delete_file_fptr delete;
    u_fs_interface_read_file_info_fptr read_file_info;
    u_fs_interface_read_fptr read;
    u_fs_interface_write_fptr write;
    u_fs_interface_repair_fs_fptr repair_fs;
    u_fs_interface_rename_file_fptr rename;
    u_fs_interface_get_childs_fptr get_childs; // NOTE: DO NOT FORGET TO FREE BUFFERS AFTER USE!

}__attribute__((aligned(16))) u_fs_interface;

u_fs_interface* _open_filesystem_interface_(uobject_ref device_ref);

#endif