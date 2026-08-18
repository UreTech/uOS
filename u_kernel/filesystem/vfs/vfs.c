#include <u_kernel/filesystem/vfs/vfs.h>
#include <u_kernel/util/u_cstr_util.h>

vfs_entry root;

void vfs_init(){
    strcpy(root.name, "root");
    root.dir_table = palloc(1); // allocate table
    memset(root.dir_table, 0x0, 4096);
    root.obj_ref = UOBJECT_NOT_DEFINED;
    root.flags = 0x0;
    root.type = VFS_TYPE_DIRECTORY;
}

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

    uart_print("dev name: ");
    uart_print(name);
    uart_print(" obj ref: ");
    uart_print_dec(object);
    uart_print("\n");

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

    uart_print("dir: ");
    uart_print(dir);
    uart_print("\n");

    vfs_entry* looking_table = ent->dir_table;
    while (looking_table != nullptr)
    {
        for (size_t j = 0; j < VFS_ENTRY_COUNT_PER_PAGE - 1; j++)
        {
            if(looking_table[j].type != VFS_TYPE_EMPTY){
                uart_print("\"");
                uart_print(looking_table[j].name);
                uart_print("\" type: ");
                uart_print_dec(looking_table[j].type);
                uart_print("\n");
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
    
}