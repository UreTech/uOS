#include <u_kernel/filesystem/vfs/vfs.h>
#include <u_kernel/util/u_cstr_util.h>
#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/objects/fsi/fsi.h>

vfs_entry root;

void vfs_init(){
    strcpy(root.name, "root");
    root.dir_table = palloc(1); // allocate table
    memset(root.dir_table, 0x0, 4096);
    root.obj_ref = UOBJECT_NOT_DEFINED;
    root.flags = 0x0;
    root.type = VFS_TYPE_DIRECTORY;
}

// not filesystem endpoint
vfs_entry* vfs_find_child(vfs_entry* parent, const char* name){
    if(parent->type != VFS_TYPE_DIRECTORY){
        udbP("VFS ERROR: Parent is not a directory:");
        udbP(parent->name);
        return nullptr;
    }

    vfs_entry* looking_table = parent->dir_table;

    while (looking_table != nullptr)
    {
        for (size_t j = 0; j < VFS_ENTRY_COUNT_PER_PAGE - 1; j++)
        {

            if(looking_table[j].type != VFS_TYPE_EMPTY && !strcmp(name, looking_table[j].name)){
                // found return
                return &looking_table[j];
            }
        }

        // get next extension table if exsists
        if(looking_table[VFS_ENTRY_COUNT_PER_PAGE - 1].type == VFS_TYPE_DIRECTORY_EXTENSION){
            looking_table = looking_table[VFS_ENTRY_COUNT_PER_PAGE - 1].dir_table;
        }else{
            looking_table = nullptr;
        }
    }
    udbP("VFS ERROR: Child does not exists:");
    udbP(name);
    return nullptr;
}
vfs_entry* vfs_find_entry(const char* path){
    
    if (path == nullptr || path[0] == '\0') return nullptr;

    // "/" = root
    if (strcmp(path, "/") == 0) return &root;

    char current_name[256];
    vfs_entry* current = &root;

    // find
    size_t finding_entry_count = parse_read_count((char*)path, '/');
    for (size_t i = 0; i < finding_entry_count; i++)
    {
        parse_read(current_name, (char*)path, '/', i);
        if(strlen(current_name, 256) == 0) continue; // empty skip

        current = vfs_find_child(current, current_name);

        if(current == nullptr){
            return nullptr;
        }else{
            if(i == (finding_entry_count - 1)){
                return current;
            }
        }
        
    }

    udbP("VFS ERROR: Should not reach here:");
    udbP(path);
    return nullptr;
}

// filesystem endpoint
typedef struct{
    vfs_entry* fsi_file;
    wide_char_t* call_path; // do not forget to free this after use
}filesystem_call_info;

filesystem_call_info vfs_find_fsi_entry(const wide_char_t* path){
    filesystem_call_info result;
    result.fsi_file = nullptr;
    result.call_path = nullptr;
    if (path == nullptr || path[0] == '\0'){ 
        udbP("VFS WARNING: File is not a file system endpoint! H0")
        return result;
    }

    wide_char_t current_name[256];
    char current_name_utf8[256];
    vfs_entry* current = &root;

    // find
    size_t finding_entry_count = w_parse_read_count(path, '/');
    for (size_t i = 0; i < finding_entry_count; i++)
    {
        w_parse_read(current_name, path, '/', i);
        if(wstrlen(current_name, 256) == 0) continue; // empty skip
        utf16_to_char_buf(current_name, current_name_utf8);
        current = vfs_find_child(current, current_name_utf8);

        if(current == nullptr){
            udbP("VFS WARNING: File is not a file system endpoint! H1");
            return result;
        }else{
            if(current->type == VFS_TYPE_FS_PARTITION){
                result.fsi_file = current;
                result.call_path = kmalloc((wstrlen(path, 8196) + 1) * sizeof(wide_char_t));
                w_parse_read_multiple(result.call_path, path, '/', i + 1, UINT64_MAX); // read to buffer
                udbPw(result.call_path);
                return result;
            }
        }
        
    }

    udbP("VFS ERROR: FSI end pount not found:");
    udbPw(path);
    result.fsi_file = nullptr;
    result.call_path = nullptr;
    return result;
}

vfs_entry* vfs_add_entry(const char* dir_path, vfs_entry entry){
    vfs_entry* parent = vfs_find_entry(dir_path);

    if(parent == nullptr){
        udbP("VFS ERROR: Adding entry failed! Parent does not exsists.");
        return FAIL;
    }

    if(parent->type != VFS_TYPE_DIRECTORY){
        udbP("VFS ERROR: Adding entry failed! Parent is not a directory.");
        return FAIL;
    }

    // find empty slot
    vfs_entry* table = parent->dir_table;
    vfs_entry* found = nullptr;
    while (table != nullptr)
    {
        for(size_t i = 0; i < VFS_ENTRY_COUNT_PER_PAGE - 1; i++){
            if(table[i].type == VFS_TYPE_EMPTY){
                found = &table[i];
                break;
            }else{
                if(!strcmp(table[i].name, entry.name)){
                    udbP("VFS ERROR: Adding entry failed! Entry with same name already exsists.");
                    return nullptr;
                }
            }
        }

        // get next extension table if exsists
        if(table[VFS_ENTRY_COUNT_PER_PAGE - 1].type == VFS_TYPE_DIRECTORY_EXTENSION){
            table = table[VFS_ENTRY_COUNT_PER_PAGE - 1].dir_table;
        }else{
            // if found break
            if(found != nullptr){
                break;
            }else{
                // create new table
                table[VFS_ENTRY_COUNT_PER_PAGE - 1].type = VFS_TYPE_DIRECTORY_EXTENSION;
                table[VFS_ENTRY_COUNT_PER_PAGE - 1].dir_table = palloc(1); // allocate table
                memset(table[VFS_ENTRY_COUNT_PER_PAGE - 1].dir_table, 0x0, 4096);
                table = table[VFS_ENTRY_COUNT_PER_PAGE - 1].dir_table;
            }
        }
    }

    *found = entry;
    return found;
}

uos_result vfs_create_directory(const char* dir_path, const char* name){
    if(vfs_check_name(name) == FAIL){
        udbP("VFS ERROR: Illegal name!");
        return FAIL;
    }

    vfs_entry entry = {};
    entry.type = VFS_TYPE_DIRECTORY;
    strcpy(entry.name, name);
    entry.dir_table = palloc(1); // allocate table
    memset(entry.dir_table, 0x0, 4096);
    entry.obj_ref = UOBJECT_NOT_DEFINED;
    entry.flags = 0x0;

    if(vfs_add_entry(dir_path, entry) == nullptr){
        udbP("VFS ERROR: An error occurred while adding entry.");
        return FAIL;
    }else{
        return SUCCESS;
    }
}

uos_result vfs_create_device(const char* dir_path, const char* name, uobject_ref object){
    if(vfs_check_name(name) == FAIL){
        udbP("VFS ERROR: Illegal name!");
        return FAIL;
    }

    vfs_entry entry = {};
    entry.type = VFS_TYPE_DEVICE;
    strcpy(entry.name, name);
    entry.dir_table = nullptr;
    entry.obj_ref = object;
    entry.flags = 0x0;

    /*
    uart_print("dev name: ");
    uart_print(name);
    uart_print(" obj ref: ");
    uart_print_dec(object);
    uart_print("\n");
    */

    if(vfs_add_entry(dir_path, entry) == nullptr){
        udbP("VFS ERROR: An error occurred while adding entry.");
        return FAIL;
    }else{
        return SUCCESS;
    }
}

uos_result vfs_create_object_file(const char* dir_path, const char* name, uobject_ref object, uint32_t type, uint32_t flags){
    if(vfs_check_name(name) == FAIL){
        udbP("VFS ERROR: Illegal name!");
        return FAIL;
    }

    vfs_entry entry = {};
    entry.type = type;
    strcpy(entry.name, name);
    entry.dir_table = nullptr;
    entry.obj_ref = object;
    entry.flags = flags;

    if(vfs_add_entry(dir_path, entry) == nullptr){
        udbP("VFS ERROR: An error occurred while adding entry.");
        return FAIL;
    }else{
        return SUCCESS;
    }
}

uobject_ref vfs_get_device_ref(const char* path){
    vfs_entry* entry = vfs_find_entry(path);

    if(entry == nullptr){
        udbP("VFS ERROR: File does not exists!");
        return UOBJECT_NOT_DEFINED;
    }

    if(entry->type != VFS_TYPE_DEVICE){
        udbP("VFS ERROR: This is not a device!");
        return UOBJECT_NOT_DEFINED;
    }

    return entry->obj_ref;
}

uos_result vfs_check_name(const char* name){
    size_t name_len = strlen(name, 1024);
    if(name_len > 103 || name_len == 0){
        udbP("VFS ERROR: Empty or over 103 character name is not allowed!");
        return FAIL;
    }

    char last = '\0';
    for(size_t i = 0; i < name_len; i++){
        switch (name[i])
        {
        case '/':
            udbP("VFS ERROR: \"/\" in name is not allowed!");
            return FAIL;
        default:
            continue;
        }
    }
    return SUCCESS;
}

void _vfs_debug_list_under_dir_(const char* dir){
    vfs_entry* ent = vfs_find_entry(dir);
    if(ent == nullptr) {
        udbP("VFS DEBUG ERROR: Directory does not exists!");
        return;
    }

    udbPs();
    udbP_STR("dir: ");
    udbP_STR(dir);
    udbPe();

    vfs_entry* looking_table = ent->dir_table;
    while (looking_table != nullptr)
    {
        for (size_t j = 0; j < VFS_ENTRY_COUNT_PER_PAGE - 1; j++)
        {
            if(looking_table[j].type != VFS_TYPE_EMPTY){
                udbPs();
                udbP_STR("\"");
                udbP_STR(looking_table[j].name);
                udbP_STR("\" type: ");
                udbP_DEC(looking_table[j].type);
                udbPe();
            }
        }

        // get next extension table if exsists
        if(looking_table[VFS_ENTRY_COUNT_PER_PAGE - 1].type == VFS_TYPE_DIRECTORY_EXTENSION){
            looking_table = looking_table[VFS_ENTRY_COUNT_PER_PAGE - 1].dir_table;
        }else{
            looking_table = nullptr;
        }
    }
}

uos_result vfs_mount_gpt_partitions(uobject_ref storage_device_obj){

    return FAIL;
}

// filesystem endpoint functions
uos_result create_file(const wide_char_t* path){
    filesystem_call_info call_info = vfs_find_fsi_entry(path);
    if(call_info.call_path == nullptr || call_info.fsi_file == nullptr){
        udbP("VFS FSI ERROR: Filesystem not found!");
        if(call_info.call_path != nullptr){
            kfree(call_info.call_path);
        }
        return FAIL;
    }

    u_fs_interface* fs = _open_filesystem_interface_(call_info.fsi_file->obj_ref);
    if(fs == nullptr){
        udbP("VFS FSI ERROR: Failed to open filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }

    _separated_path sep_path = separate_path(call_info.call_path);
    if(sep_path.file_name == nullptr || sep_path.path == nullptr){
        if(sep_path.file_name != nullptr){
            kfree(sep_path.file_name);
        }
        if(sep_path.path != nullptr){
            kfree(sep_path.path);
        }
        udbP("VFS FSI ERROR: Failed to parse path!");
        kfree(call_info.call_path);
        return FAIL;
    }

    uos_result op_res = fs->create(fs, sep_path.path, sep_path.file_name);
    
    kfree(sep_path.file_name);
    kfree(sep_path.path);

    if(uobject_close_object(call_info.fsi_file->obj_ref) != SUCCESS){
        udbP("VFS FSI ERROR: Failed to close filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }

    kfree(call_info.call_path);
    return op_res;
}

uos_result create_directory(const wide_char_t* path){
    filesystem_call_info call_info = vfs_find_fsi_entry(path);
    if(call_info.call_path == nullptr || call_info.fsi_file == nullptr){
        udbP("VFS FSI ERROR: Filesystem not found!");
        if(call_info.call_path == nullptr ){
            udbP("zorortortrotror");
        }
        if(call_info.call_path != nullptr){
            kfree(call_info.call_path);
        }
        return FAIL;
    }

    u_fs_interface* fs = _open_filesystem_interface_(call_info.fsi_file->obj_ref);
    if(fs == nullptr){
        udbP("VFS FSI ERROR: Failed to open filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }

    _separated_path sep_path = separate_path(call_info.call_path);
    if(sep_path.file_name == nullptr || sep_path.path == nullptr){
        if(sep_path.file_name != nullptr){
            kfree(sep_path.file_name);
        }
        if(sep_path.path != nullptr){
            kfree(sep_path.path);
        }
        udbP("VFS FSI ERROR: Failed to parse path!");
        kfree(call_info.call_path);
        return FAIL;
    }

    uos_result op_res = fs->create_dir(fs, sep_path.path, sep_path.file_name);
    kfree(sep_path.file_name);
    kfree(sep_path.path);

    if(uobject_close_object(call_info.fsi_file->obj_ref) != SUCCESS){
        udbP("VFS FSI ERROR: Failed to close filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }
    
    kfree(call_info.call_path);
    return op_res;
}

uos_result delete(const wide_char_t* path){
    filesystem_call_info call_info = vfs_find_fsi_entry(path);
    if(call_info.call_path == nullptr || call_info.fsi_file == nullptr){
        udbP("VFS FSI ERROR: Filesystem not found!");
        if(call_info.call_path != nullptr){
            kfree(call_info.call_path);
        }
        return FAIL;
    }

    u_fs_interface* fs = _open_filesystem_interface_(call_info.fsi_file->obj_ref);
    if(fs == nullptr){
        udbP("VFS FSI ERROR: Failed to open filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }

    uos_result op_res = fs->delete(fs, call_info.call_path);

    if(uobject_close_object(call_info.fsi_file->obj_ref) != SUCCESS){
        udbP("VFS FSI ERROR: Failed to close filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }

    kfree(call_info.call_path);
    return op_res;
}

u_fs_file_info read_info(const wide_char_t* path){
    filesystem_call_info call_info = vfs_find_fsi_entry(path);
    u_fs_file_info op_res = {};
    op_res.flags = FSI_FLAG_INVALID_BIT;
    if(call_info.call_path == nullptr || call_info.fsi_file == nullptr){
        udbP("VFS FSI ERROR: Filesystem not found!");
        if(call_info.call_path != nullptr){
            kfree(call_info.call_path);
        }
        return op_res;
    }

    u_fs_interface* fs = _open_filesystem_interface_(call_info.fsi_file->obj_ref);
    if(fs == nullptr){
        udbP("VFS FSI ERROR: Failed to open filesystem interface!");
        kfree(call_info.call_path);
        return op_res;
    }

    op_res = fs->read_info(fs, call_info.call_path);

    if(uobject_close_object(call_info.fsi_file->obj_ref) != SUCCESS){
        udbP("VFS FSI ERROR: Failed to close filesystem interface!");
        kfree(call_info.call_path);
        return op_res;
    }

    kfree(call_info.call_path);
    return op_res;
}

uos_result read(const wide_char_t* file_path, size_t read_offset, size_t read_len, void* read_buffer){
    filesystem_call_info call_info = vfs_find_fsi_entry(file_path);
    if(call_info.call_path == nullptr || call_info.fsi_file == nullptr){
        udbP("VFS FSI ERROR: Filesystem not found!");
        if(call_info.call_path != nullptr){
            kfree(call_info.call_path);
        }
        return FAIL;
    }

    u_fs_interface* fs = _open_filesystem_interface_(call_info.fsi_file->obj_ref);
    if(fs == nullptr){
        udbP("VFS FSI ERROR: Failed to open filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }

    uos_result op_res = fs->read(fs, call_info.call_path, read_offset, read_len, read_buffer);

    if(uobject_close_object(call_info.fsi_file->obj_ref) != SUCCESS){
        udbP("VFS FSI ERROR: Failed to close filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }

    kfree(call_info.call_path);
    return op_res;
}

uos_result write(const wide_char_t* file_path, size_t write_offset, size_t write_len, void* write_buffer){
    filesystem_call_info call_info = vfs_find_fsi_entry(file_path);
    if(call_info.call_path == nullptr || call_info.fsi_file == nullptr){
        udbP("VFS FSI ERROR: Filesystem not found!");
        if(call_info.call_path != nullptr){
            kfree(call_info.call_path);
        }
        return FAIL;
    }

    u_fs_interface* fs = _open_filesystem_interface_(call_info.fsi_file->obj_ref);
    if(fs == nullptr){
        udbP("VFS FSI ERROR: Failed to open filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }

    uos_result op_res = fs->write(fs, call_info.call_path, write_offset, write_len, write_buffer);

    if(uobject_close_object(call_info.fsi_file->obj_ref) != SUCCESS){
        udbP("VFS FSI ERROR: Failed to close filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }

    kfree(call_info.call_path);
    return op_res;
}

uos_result copy(const wide_char_t* src_path, const wide_char_t* dst_path){
    filesystem_call_info src_call_info = vfs_find_fsi_entry(src_path);
    if(src_call_info.call_path == nullptr || src_call_info.fsi_file == nullptr){
        udbP("VFS FSI ERROR: Source filesystem not found!");
        if(src_call_info.call_path != nullptr){
            kfree(src_call_info.call_path);
        }
        return FAIL;
    }

    filesystem_call_info dst_call_info = vfs_find_fsi_entry(dst_path);
    if(dst_call_info.call_path == nullptr || dst_call_info.fsi_file == nullptr){
        udbP("VFS FSI ERROR: Destination filesystem not found!");
        if(src_call_info.call_path != nullptr){
            kfree(src_call_info.call_path);
        }
        if(dst_call_info.call_path != nullptr){
            kfree(dst_call_info.call_path);
        }
        return FAIL;
    }

    if(src_call_info.fsi_file->obj_ref == dst_call_info.fsi_file->obj_ref){
        // copy in same filesystem instance
        u_fs_interface* src_fs = _open_filesystem_interface_(src_call_info.fsi_file->obj_ref);
        if(src_fs == nullptr){
            udbP("VFS FSI ERROR: Failed to open source filesystem interface!");
            if(src_call_info.call_path != nullptr){
                kfree(src_call_info.call_path);
            }
            if(dst_call_info.call_path != nullptr){
                kfree(dst_call_info.call_path);
            }
            return FAIL;
        }

        uos_result op_res = src_fs->copy(src_fs, src_call_info.call_path, dst_call_info.call_path);

        if(uobject_close_object(src_call_info.fsi_file->obj_ref) != SUCCESS){
            udbP("VFS FSI ERROR: Failed to close filesystem interface!");
            kfree(src_call_info.call_path);
            kfree(dst_call_info.call_path);
            return FAIL;
        }

        kfree(src_call_info.call_path);
        kfree(dst_call_info.call_path);
        return op_res;
    }else{
        // copy in different filesystem
        
        u_fs_file_info src_info = read_info(src_path);
        if(src_info.flags & FSI_FLAG_INVALID_BIT){
            if(src_call_info.call_path != nullptr){
                kfree(src_call_info.call_path);
            }
            if(dst_call_info.call_path != nullptr){
                kfree(dst_call_info.call_path);
            }
            udbP("VFS FSI ERROR: Source file is invalid!");
            return FAIL;
        }

        if(src_info.flags & FSI_FLAG_DIRECTORY_BIT){
            // NOTE: COPYING DIRECTORIES BETWEEN DIFFERENT FILESYSTEM INSTANCES IS NOT ALLOWED
            udbP("VFS FSI ERROR: Copying directories between filesystems is not allowed! NOTE: Use manuel solutions.");
            if(src_call_info.call_path != nullptr){
                kfree(src_call_info.call_path);
            }
            if(dst_call_info.call_path != nullptr){
                kfree(dst_call_info.call_path);
            }
            return FAIL;
        }else{
            uint8_t* buf = kmalloc(SIZE_1K * 256); // 256kb buffer for copying file
            size_t remaining_size = src_info.file_size;

            if(create_file(dst_path) != SUCCESS){
                kfree(src_call_info.call_path);
                kfree(dst_call_info.call_path);
                kfree(buf);
                udbP("VFS FSI ERROR: Failed to copy! H0");
                return FAIL;
            }

            while (remaining_size)
            {
                size_t read_size = (SIZE_1K * 256);
                if(remaining_size < (SIZE_1K * 256)){
                    read_size = remaining_size;
                }

                // read
                if(read(src_path, src_info.file_size - remaining_size, read_size, buf) != SUCCESS){
                    kfree(src_call_info.call_path);
                    kfree(dst_call_info.call_path);
                    kfree(buf);
                    udbP("VFS FSI ERROR: Failed to copy! H1");
                    return FAIL;
                }

                // write
                if(write(dst_path, src_info.file_size - remaining_size, read_size, buf) != SUCCESS){
                    kfree(src_call_info.call_path);
                    kfree(dst_call_info.call_path);
                    kfree(buf);
                    udbP("VFS FSI ERROR: Failed to copy! H2");
                    return FAIL;
                }
                remaining_size -= read_size;
            }

            kfree(src_call_info.call_path);
            kfree(dst_call_info.call_path);
            kfree(buf);
            return SUCCESS;
        }
    }
}

uos_result move(const wide_char_t* src_path, const wide_char_t* dst_path){
    filesystem_call_info src_call_info = vfs_find_fsi_entry(src_path);
    if(src_call_info.call_path == nullptr || src_call_info.fsi_file == nullptr){
        udbP("VFS FSI ERROR: Source filesystem not found!");
        if(src_call_info.call_path != nullptr){
            kfree(src_call_info.call_path);
        }
        return FAIL;
    }

    filesystem_call_info dst_call_info = vfs_find_fsi_entry(dst_path);
    if(dst_call_info.call_path == nullptr || dst_call_info.fsi_file == nullptr){
        udbP("VFS FSI ERROR: Destination filesystem not found!");
        if(src_call_info.call_path != nullptr){
            kfree(src_call_info.call_path);
        }
        if(dst_call_info.call_path != nullptr){
            kfree(dst_call_info.call_path);
        }
        return FAIL;
    }

    if(src_call_info.fsi_file->obj_ref == dst_call_info.fsi_file->obj_ref){
        // move in same filesystem instance
        u_fs_interface* src_fs = _open_filesystem_interface_(src_call_info.fsi_file->obj_ref);
        if(src_fs == nullptr){
            udbP("VFS FSI ERROR: Failed to open source filesystem interface!");
            if(src_call_info.call_path != nullptr){
                kfree(src_call_info.call_path);
            }
            if(dst_call_info.call_path != nullptr){
                kfree(dst_call_info.call_path);
            }
            return FAIL;
        }

        uos_result op_res = src_fs->move(src_fs, src_call_info.call_path, dst_call_info.call_path);

        if(uobject_close_object(src_call_info.fsi_file->obj_ref) != SUCCESS){
            udbP("VFS FSI ERROR: Failed to close filesystem interface!");
            kfree(src_call_info.call_path);
            kfree(dst_call_info.call_path);
            return FAIL;
        }

        kfree(src_call_info.call_path);
        kfree(dst_call_info.call_path);
        return op_res;
    }else{
        // move in different filesystem
        
        u_fs_file_info src_info = read_info(src_path);
        if(src_info.flags & FSI_FLAG_INVALID_BIT){
            if(src_call_info.call_path != nullptr){
                kfree(src_call_info.call_path);
            }
            if(dst_call_info.call_path != nullptr){
                kfree(dst_call_info.call_path);
            }
            udbP("VFS FSI ERROR: Source file is invalid!");
            return FAIL;
        }

        if(src_info.flags & FSI_FLAG_DIRECTORY_BIT){
            // NOTE: MOVING DIRECTORIES BETWEEN DIFFERENT FILESYSTEM INSTANCES IS NOT ALLOWED
            udbP("VFS FSI ERROR: Moving directories between filesystems is not allowed! NOTE: Use manuel solutions.");
            if(src_call_info.call_path != nullptr){
                kfree(src_call_info.call_path);
            }
            if(dst_call_info.call_path != nullptr){
                kfree(dst_call_info.call_path);
            }
            return FAIL;
        }else{
            // this are not used after here
            kfree(src_call_info.call_path);
            kfree(dst_call_info.call_path);

            // first copy to destination
            if(copy(src_path, dst_path) == SUCCESS){
                // delete old one
                return delete(src_path);
            }else{
                return FAIL;
            }
        }
    }
}

uos_result resize_file(const wide_char_t* file_path, size_t new_size){
    filesystem_call_info call_info = vfs_find_fsi_entry(file_path);
    if(call_info.call_path == nullptr || call_info.fsi_file == nullptr){
        udbP("VFS FSI ERROR: Filesystem not found!");
        if(call_info.call_path == nullptr ){
            udbP("zorortortrotror");
        }
        if(call_info.call_path != nullptr){
            kfree(call_info.call_path);
        }
        return FAIL;
    }

    u_fs_interface* fs = _open_filesystem_interface_(call_info.fsi_file->obj_ref);
    if(fs == nullptr){
        udbP("VFS FSI ERROR: Failed to open filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }

    uos_result op_res = fs->resize_file(fs, call_info.call_path, new_size);

    if(uobject_close_object(call_info.fsi_file->obj_ref) != SUCCESS){
        udbP("VFS FSI ERROR: Failed to close filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }
    
    kfree(call_info.call_path);
    return op_res;
}

uos_result get_dir_childs(const wide_char_t* dir_path, wide_char_t*** child_names, size_t* names_buffer_size){
    filesystem_call_info call_info = vfs_find_fsi_entry(dir_path);
    if(call_info.call_path == nullptr || call_info.fsi_file == nullptr){
        udbP("VFS FSI ERROR: Filesystem not found!");
        if(call_info.call_path != nullptr){
            kfree(call_info.call_path);
        }
        return FAIL;
    }

    u_fs_interface* fs = _open_filesystem_interface_(call_info.fsi_file->obj_ref);
    if(fs == nullptr){
        udbP("VFS FSI ERROR: Failed to open filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }

    uos_result op_res = fs->get_childs(fs, call_info.call_path, child_names, names_buffer_size);

    if(uobject_close_object(call_info.fsi_file->obj_ref) != SUCCESS){
        udbP("VFS FSI ERROR: Failed to close filesystem interface!");
        kfree(call_info.call_path);
        return FAIL;
    }

    kfree(call_info.call_path);
    return op_res;
}

