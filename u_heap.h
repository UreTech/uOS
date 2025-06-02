#include"u_ctypes.h"
#ifndef U_HEAP_H
#define U_HEAP_H
extern uint8_t __heap_base;
extern uint8_t __heap_end;

static void* heap_start = (uint64_t*)&__heap_base;
static void* heap_limit = (uint64_t*)&__heap_end;

static size_t u_total_heap_size;

#define U_HEAP_FREE_BLOCK 0x07
#define U_HEAP_ALLOCATED_BLOCK 0x70

#define U_HEAP_ALLOC_REASON_UNKNOWN 0x00
#define U_HEAP_ALLOC_REASON_APP     0x01
#define U_HEAP_ALLOC_REASON_UXC     0x02
#define U_HEAP_ALLOC_REASON_KERNEL  0x0f

void _u_heap_init();

void* u_malloc(size_t size);

void u_free(void* block);

size_t _u_free_heap();

size_t _u_allocated_block_count();

void memset(void* ptr, uint8_t value, size_t size);

void memcpy(void* dst, void *src, size_t len);

int memcmp(void* src1, void *src2, size_t len);

#endif