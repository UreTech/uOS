#pragma once

#include <u_kernel/memory/u_heap.h>

// heap contexts
extern UHP_CONTEXT base_kernel_heap_context;
extern UHP_CONTEXT base_kernel_vheap_context;

void init_kernel_base_heaps();

// base kernel heap allocator
void *kmalloc(size_t size);
void *kreloc(void *oldBlock, size_t oldSize, size_t newSize);
void kfree(void *block);

// base kernel video heap allocator
void *kvmalloc(size_t size);
void *kvreloc(void *oldBlock, size_t oldSize, size_t newSize);
void kvfree(void *block);

// page allocator
void* palloc(size_t page_count);
void pfree(void* first_page, size_t page_count );

// video page allocator
void* vpalloc(size_t page_count);
void vpfree(void* first_page, size_t page_count );

// standart util
void memcpy(void *dst, void *src, size_t size);
void memset(void *dst, uint8_t val, size_t size);
void memfill(volatile void *ptr, void *filler, size_t fSize, size_t times);
int memcmp(void *src1, void *src2, size_t len);
