#ifndef FSI_H
#define FSI_H

#include <u_kernel/
#include <u_kernel/objects/uobject.h>

typedef uos_result (*u_fs_interface_open_file_fptr)(const char* path);
typedef uos_result (*u_fs_interface_close_file_fptr)(const char* path);
typedef uos_result (*u_fs_interface_repair_fs_fptr)();
typedef uos_result (*u_fs_interface_rename_file_fptr)(const char* path, const char* new_name);


typedef struct{
    char fs_type_name[48];
    uobject_ref storage_device;
    u_fs_interface_open_file_fptr open;
    u_fs_interface_close_file_fptr close;
    u_fs_interface_repair_fs_fptr repair_fs;
    u_fs_interface_rename_file_fptr rename;
}__attribute__((aligned(16))) u_fs_interface;

void gio(){
    sizeof(u_fs_interface);
}

#endif