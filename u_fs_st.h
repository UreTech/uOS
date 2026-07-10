#ifndef U_FS_ST_H
#define U_FS_ST_H
#include "u_ctypes.h"

#define UFS_MAX_FILE_ENTRIES (1024 * 60)
#define UFS_MAX_FILE_ENTRY_BLOCKS (UFS_MAX_FILE_ENTRIES / 2)

#define UFS_MAX_FREE_TABLE_BLOCKS 32768 // ~= 64Gb max mapping

typedef struct
{
	char ufs_ver[7];

	// free table (max 32768 ~= 64Gb max mapping)
	size_t free_table_offset;
	size_t usable_block_count;

	// entry table
	size_t entry_table_offset;
	size_t entry_count;
} ufs_fs_head_;

typedef uint64_t ufs_entry_id;
typedef uint64_t ufs_block_id;

// *** entry item ***
typedef uint64_t ufs_entry_flags_;
#define UFS_ENT_FLAG_NULL_BIT ONEBIT(0)
#define UFS_ENT_FLAG_FILE_BIT ONEBIT(1)
#define UFS_ENT_FLAG_FOLDER_BIT ONEBIT(2)
#define UFS_ENT_FLAG_FOLDER_EXTENSION_BIT ONEBIT(3)

#define UFS_MAX_CHILDREN 24

typedef struct
{
	ufs_entry_flags_ flags;	   // 1
	char name[32];			   // 4
	ufs_entry_id parent;	   // 1
	size_t children_count;	   // 1
	ufs_entry_id children[24]; //24

	ufs_block_id data; // 1
} ufs_entry_;
// 2 entries per block

typedef struct
{
	ufs_entry_ ent[2];
} ufs_entry_block_;
// *******
#endif