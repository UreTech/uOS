#ifndef UOBJECT_H
#define UOBJECT_H

#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/util/lock/u_mutex.h>
#include <u_kernel/objects/udevice/udevice.h>

#define UOBJECT_MAXIMUM_OBJECT_COUNT (2048ULL)
#define UOBJECT_NOT_DEFINED (0xDEADBEEFULL)
#define UOBJECT_MAX_NAME_LEN (47ULL) // null term excluded

// object type ids
// this ids points the index of table
#define UOBJECT_TYPE_EMPTY (0ULL) // empty flag
#define UOBJECT_TYPE_NULL (1ULL) // no table assigned
#define UOBJECT_TYPE_UHANDLE (2ULL) // handle table
#define UOBJECT_TYPE_DEVICE (3ULL) // udevice table
#define UOBJECT_TYPE_CUSTOM (4ULL) // custom object table

// base kernel object
// 256 bytes (32 entry per page)
typedef struct{
    uint64_t type; // type id
    uint64_t kernel_idx; // index in table (not used)
    uint64_t flags; // flags
    u_mutex ref_lock; // 8 byte refrance lock
    uint64_t ref_count; // count of referances for garbage collection
    char name[48]; // object name
    uint8_t padding[8];
    uint8_t obj_data[256 - (32 + 48 + 8 + 8)]; // object specific data (16 byte aligned)
}uobject;
#define UOBJECT_MAX_DATA_SIZE (160ULL)

typedef uint64_t uobject_ref;

void uobject_init_uobject_tables();

uobject_ref uobject_create_null(const char* name, uint64_t flags);

uobject_ref uobject_create_custom(const char* name, uint64_t flags, uint8_t* data, size_t data_size);

uobject_ref uobject_create_udevice(const char* name, uint64_t flags, udevice device_header);


#endif