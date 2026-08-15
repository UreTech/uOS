#ifndef VFS_H
#define VFS_H

#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/util/lock/u_mutex.h>
#include <u_kernel/memory/u_memory.h>
#include <u_kernel/objects/uobject.h>

#define VFS_TYPE_EMPTY (0ULL)
#define VFS_TYPE_NULL (1ULL)
#define VFS_TYPE_STORAGE (2ULL)
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

vfs_entry* vfs_find_entry(const char* path);
vfs_entry* vfs_add_entry(const char* dir_path, vfs_entry entry);

uos_result vfs_check_name(const char* name);

#endif