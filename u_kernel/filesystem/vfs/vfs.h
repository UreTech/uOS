#ifndef VFS_H
#define VFS_H

#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/util/lock/u_mutex.h>
#include <u_kernel/memory/u_memory.h>
#include <u_kernel/objects/uobject.h>
#include <u_kernel/objects/fsi/fsi.h>

#define VFS_TYPE_EMPTY (0ULL)
#define VFS_TYPE_NULL (1ULL)
#define VFS_TYPE_FS_PARTITION    (2ULL)
#define VFS_TYPE_SYSTEM (3ULL)
#define VFS_TYPE_DEVICE (4ULL)
#define VFS_TYPE_DIRECTORY (5ULL)
#define VFS_TYPE_DIRECTORY_EXTENSION (6ULL)

typedef struct vfs_entry
{
    uint32_t type;
    uint32_t flags;
    struct vfs_entry* dir_table;
    uobject_ref obj_ref;
    char name[128 - 24];
}vfs_entry;
_Static_assert(sizeof(vfs_entry) == 128, "vfs_entry must be 128 bytes");

#define VFS_ENTRY_COUNT_PER_PAGE (4096ULL / sizeof(vfs_entry))

void vfs_init();
uos_result vfs_create_directory(const char* dir_path, const char* name);
uos_result vfs_create_device(const char* dir_path, const char* name, uobject_ref object);
uos_result vfs_create_object_file(const char* dir_path, const char* name, uobject_ref object, uint32_t type, uint32_t flags);

uobject_ref vfs_get_device_ref(const char* path);

vfs_entry* vfs_find_entry(const char* path);
vfs_entry* vfs_add_entry(const char* dir_path, vfs_entry entry);

uos_result vfs_check_name(const char* name);

uos_result vfs_mount_gpt_partitions(uobject_ref storage_device_obj);

// filesystem endpoint functions
uos_result create_file(const char* file_path, const char* file_name);
uos_result create_directory(const char* dir_path, const char* dir_name);
uos_result delete_file(const char* file_path);
u_fs_file_info read_file_info(const char* file_path);
uos_result read(const char* file_path, size_t read_offset, size_t read_len, void* read_buffer);
uos_result write(const char* file_path, size_t write_offset, size_t write_len, void* write_buffer);
uos_result rename(const char* file_path, const char* new_name);
uos_result get_dir_childs(const char* dir_path, const char** child_names, size_t names_buffer_size); // NOTE: DO NOT FORGET TO FREE BUFFERS AFTER USE!

// debug
void _vfs_debug_list_under_dir_(const char* dir);

#endif