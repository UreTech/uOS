#include <u_kernel/objects/fsi/fsi.h>

u_fs_interface* _open_filesystem_interface_(uobject_ref device_ref){
    uobject* interface_object = uobject_open_object(device_ref, UOBJECT_TYPE_FSI);  
    if(interface_object == nullptr){
        udbP("FS STORAGE DEVICE ERROR: Failed to open filesystem interface!");
        return nullptr;
    }
    
    u_fs_interface* interface = (u_fs_interface*)(interface_object->obj_data);
    return interface;
}