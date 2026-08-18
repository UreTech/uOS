#include <u_kernel/objects/uobject.h>
#include <u_kernel/memory/u_memory.h>
#include <u_kernel/util/u_cstr_util.h>

u_mutex object_table_lock;
uobject* object_table = nullptr;

void uobject_init_uobject_tables(){
    object_table = kmalloc(sizeof(uobject) * UOBJECT_MAXIMUM_OBJECT_COUNT);
    memset(object_table, 0x0, sizeof(uobject) * UOBJECT_MAXIMUM_OBJECT_COUNT);

    // reset locks & indicies
    for(size_t i = 0; i < UOBJECT_MAXIMUM_OBJECT_COUNT; i++){
        object_table[i].kernel_idx = UOBJECT_NOT_DEFINED;
        object_table[i].type = UOBJECT_TYPE_EMPTY;
        u_mutex_reset(&(object_table[i].ref_lock));
    }
    u_mutex_reset(&object_table_lock); // reset table lock
}

uobject_ref uobject_create(uint64_t type, const char* name, uint64_t flags, uint8_t* data, size_t data_size){
    if(strlen(name, 47) == 0){
        udbP("UOBJECT ERROR: Object name should not be empty or longer than 47 chars!");
        return UOBJECT_NOT_DEFINED;
    }

    if(data_size > UOBJECT_MAX_DATA_SIZE){
        udbP("UOBJECT ERROR: Object data size should not be over UOBJECT_MAX_DATA_SIZE!");
        return UOBJECT_NOT_DEFINED;
    }

    u_mutex_lock_el1(&object_table_lock); // lock

    // find empty
    uint64_t found = UOBJECT_NOT_DEFINED;
    for(size_t i = 0; i < UOBJECT_MAXIMUM_OBJECT_COUNT; i++){
        if(object_table[i].type == UOBJECT_TYPE_EMPTY){
            found = i;
            break;
        }
    }

    if(found == UOBJECT_NOT_DEFINED){
        u_mutex_unlock_el1(&object_table_lock); // unlock
        udbP("UOBJECT ERROR: No empty slot found!");
        return UOBJECT_NOT_DEFINED;
    }

    object_table[found].type = type; // type is set so slot is allocated and we can release the lock
    u_mutex_unlock_el1(&object_table_lock); // unlock

    object_table[found].flags = flags;
    object_table[found].kernel_idx = UOBJECT_NOT_DEFINED; // not used
    strcpy(object_table[found].name, name);
    // write obj_data if exsists
    if(data_size){
        memcpy(object_table[found].obj_data, data, data_size);
    }

    return found;
}

uobject_ref uobject_create_null(const char* name, uint64_t flags){
   return uobject_create(UOBJECT_TYPE_NULL, name, flags, nullptr, 0);
}

uobject_ref uobject_create_udevice(const char* name, uint64_t flags, udevice device_header){
   return uobject_create(UOBJECT_TYPE_DEVICE, name, flags, (uint8_t*)&device_header, sizeof(udevice));
}

uobject* uobject_open_object(uobject_ref ref, uint64_t object_type){

    // WARNING: There is should be a ref check for not opening same thing twice

    if(ref >= UOBJECT_MAXIMUM_OBJECT_COUNT){
        udbP("UOBJECT ERROR: Invalid object referance!");
        return nullptr;
    }

    u_mutex_lock_el1(&object_table_lock); // lock

    if(object_table[ref].type != object_type){
        u_mutex_unlock_el1(&object_table_lock); // unlock
        udbP("UOBJECT ERROR: Object type mismatch!");
        return nullptr;
    }

    u_mutex_lock_el1(&(object_table[ref].ref_lock)); // lock ref counter
    object_table[ref].ref_count++;
    u_mutex_unlock_el1(&(object_table[ref].ref_lock)); // unlock ref counter

    u_mutex_unlock_el1(&object_table_lock); // unlock

    return &object_table[ref];
}

uos_result uobject_close_object(uobject_ref ref){
    // WARNING: There is should be a ref check for not closing same thing twice

        if(ref >= UOBJECT_MAXIMUM_OBJECT_COUNT){
        udbP("UOBJECT ERROR: Invalid object referance!");
        return FAIL;
    }

    u_mutex_lock_el1(&object_table_lock); // lock

    u_mutex_lock_el1(&(object_table[ref].ref_lock)); // lock ref counter
    object_table[ref].ref_count--;
    u_mutex_unlock_el1(&(object_table[ref].ref_lock)); // unlock ref counter

    u_mutex_unlock_el1(&object_table_lock); // unlock

    return SUCCESS;
}