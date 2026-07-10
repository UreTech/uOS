#pragma once
#include <memory/u_mempage.h>

// slab allocator based combine heap

#define UHP_FREE 0
#define UHP_ALLOCATED 1

#define UHP_PAGE_SIZE (0x1000) // 4kb

#define UHP_SLAB_BLOCK_PAGE_COUNT 2 // 8kb

#define UHP_SLAB_BLOCK_SIZE (UHP_SLAB_BLOCK_PAGE_COUNT * UHP_PAGE_SIZE) // 8kb

#define UHP_MAX_SLAB_SIZE (2048 + 128 + 32)

struct slab_block_header;

typedef struct slab_block_header
{
	uint8_t *freeTable;
	uint8_t *data;
	uint64_t slab_size;
	uint64_t slab_count;
	uint64_t free_slab_count;
	uint64_t free_table_size;

	uint64_t page_count;
	struct slab_block_header *next;

	// 64byte alignment
} slab_block_header;

#define MEMORY_REGION_MMR 0
#define MEMORY_REGION_VMMR 1

typedef struct
{
	uint64_t total_heap_size;
	uint64_t allocated_block_count;
	uint64_t max_block_count;
	uint8_t memory_region; // WARNING! Do not change this value in runtime it could cause kernel panic or even kernel state/heap corruption!

	slab_block_header *block_list;
} UHP_CONTEXT;

// internal command
slab_block_header *create_slab(size_t slab_size, slab_block_header *parent, UHP_CONTEXT *ctx);


void UHP_init_heap_context(UHP_CONTEXT *ctx, uint8_t _memory_region);

void *UHP_malloc(size_t size, UHP_CONTEXT *ctx);

void UHP_free(void *slab, UHP_CONTEXT *ctx);

// Note: This function returns summary of all free slabs
size_t UHP_query_free_slabs(UHP_CONTEXT *ctx);

// mostly same as UHP_ query_free_slabs() except filtering
size_t UHP_query_free_slabs_with_filter(UHP_CONTEXT *ctx, uint64_t searching_slab_size);
