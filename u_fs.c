#include "u_fs.h"
#include "u_fs_st.h"

int ufs_init_sd()
{
	uint8_t *headblock = (uint8_t *)kmalloc(512);

	int r = sd_read_block(0, headblock);

	if (r != SD_OK)
	{
		uart_print("[E00] UFS init fail with code: ");
		uart_print_dec(r);
		uart_print("\n");

		kfree(headblock);
		return UFS_FAIL;
	}

	ufs_fs_head_ *head = (ufs_fs_head_ *)headblock;

	head->ufs_ver[6] = '\0';

	if (strcmp(head->ufs_ver, UFS_VER_STR))
	{
		uart_print("[E01] UFS init failed! UFS head is invalid: ");
		uart_print(head->ufs_ver);
		uart_print("\n");

		kfree(headblock);
		return UFS_FAIL;
	}

	kfree(headblock);
	return UFS_SUCCESS;
}

int ufs_format_sd()
{
	int r = SD_OK;
	uint64_t current_offset = 0;

	current_offset++; // head size

	uint32_t block_count = sd_get_block_count();

	// max - header - entry table - free table
	uint32_t useable_blocks = block_count - 1 - UFS_MAX_FILE_ENTRY_BLOCKS - UFS_MAX_FREE_TABLE_BLOCKS;

	// align to 512 block size
	uint32_t block_count_of_free_table = ((useable_blocks + 0x1FF) & ~(0x1FF)) / 512;

	// dummy full free buffer
	uint8_t *fullfree = (uint8_t *)kmalloc(512);
	memset(fullfree, 0xFF, 512); // true = block is free

	// buffer for multiple block write
	uint8_t *mul_ff = (uint8_t *)kmalloc(512 * block_count_of_free_table);

	// copy data
	uart_print("Buffering\n");
	memfill(mul_ff, fullfree, 512, block_count_of_free_table);
	kfree(fullfree); // its job done

	// debug
	time_point start;
	start = get_now();

	// multi write
	uart_print("Writing\n");
	r = sd_write_multiple_blocks(current_offset, mul_ff, block_count_of_free_table);

	// debug
	uart_print("Creating free table finish in: ");
	uart_print_float((float)tp_to_ms(get_now() - start) / 1000.0, 2);
	uart_print(" seconds\n");

	if (r != SD_OK)
	{
		uart_print("[E02] UFS format fail with code: ");
		uart_print_dec(r);
		uart_print("\n");

		kfree(mul_ff);
		return UFS_FAIL;
	}
	kfree(mul_ff); //its job done

	current_offset += UFS_MAX_FREE_TABLE_BLOCKS; // free table region

	// dummy free directory buffer
	uint8_t *free_entries = (uint8_t *)kmalloc(512);

	ufs_entry_block_ *entries = (ufs_entry_block_ *)free_entries;

	entries->ent[0].flags = UFS_ENT_FLAG_NULL_BIT; // set as null (empty)
	entries->ent[1].flags = UFS_ENT_FLAG_NULL_BIT; // set as null (empty)

	// buffer for multiple block write
	uint8_t *mul_et = (uint8_t *)kmalloc(512 * UFS_MAX_FILE_ENTRY_BLOCKS);

	// copy data
	uart_print("Buffering\n");
	memfill(mul_et, free_entries, 512, UFS_MAX_FILE_ENTRY_BLOCKS);
	kfree(free_entries); // its job done

	// debug
	start = get_now();

	// multi write
	uart_print("Writing\n");
	r = sd_write_multiple_blocks(current_offset, mul_et, UFS_MAX_FILE_ENTRY_BLOCKS);

	// debug
	uart_print("Creating entry table finish in: ");
	uart_print_float((float)tp_to_ms(get_now() - start) / 1000.0, 2);
	uart_print(" seconds\n");

	if (r != SD_OK)
	{
		uart_print("[E03] UFS format fail with code: ");
		uart_print_dec(r);
		uart_print("\n");

		kfree(mul_et);
		return UFS_FAIL;
	}
	udb();										 // giogiogio
	kfree(mul_et);								 //its job done
	udb();										 // giogiogio
	current_offset += UFS_MAX_FILE_ENTRY_BLOCKS; // free table region
	udb();										 // giogiogio
	uint8_t *headblock = (uint8_t *)kmalloc(512);
	udb(); // giogiogio
	ufs_fs_head_ *head = (ufs_fs_head_ *)headblock;
	udb(); // giogiogio
	strcpy(head->ufs_ver, UFS_VER_STR);
	udb(); // giogiogio
	// set header information
	head->free_table_offset = 1; // right after head
	head->usable_block_count = useable_blocks;
	udb();													  // giogiogio
	head->entry_table_offset = 1 + UFS_MAX_FREE_TABLE_BLOCKS; // right after free table
	udb();													  // giogiogio
	head->entry_count = UFS_MAX_FILE_ENTRIES;
	udb(); // giogiogio
	// write head
	r = sd_write_block(0, headblock);
	udb(); // giogiogio
	if (r != SD_OK)
	{
		uart_print("[E04] UFS format fail with code: ");
		uart_print_dec(r);
		uart_print("\n");

		kfree(headblock);
		return UFS_FAIL;
	}
	udb(); // giogiogio
	kfree(headblock);
	udb(); // giogiogio
	return UFS_SUCCESS;
}