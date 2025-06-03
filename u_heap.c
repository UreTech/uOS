#include"u_heap.h"
#include"u_display.h"
typedef struct BlockHeader {
    size_t size;
    struct BlockHeader* next;
    uint8_t status;
    uint8_t alloc_reason;
} BlockHeader;

#define ALIGN16(x) (((x) + 15) & ~15)
#define HEADER_SIZE sizeof(BlockHeader)

static BlockHeader* free_list = NULL;

void _u_heap_init() {
    u_total_heap_size = (u64)heap_limit - (u64)heap_start;
    free_list = (BlockHeader*)heap_start;
    free_list->size = u_total_heap_size - HEADER_SIZE;
    free_list->next = NULL;
    free_list->status = U_HEAP_FREE_BLOCK;
    free_list->alloc_reason = U_HEAP_ALLOC_REASON_KERNEL;
}

void* u_malloc(size_t size) {
    if (size == 0) return NULL;

    size = ALIGN16(size); // 16 byte hizalama
    BlockHeader* current = free_list;
    BlockHeader* prev = NULL;

    while (current) {
        if (current->status == U_HEAP_FREE_BLOCK && current->size >= size) {
            // Gerekirse blok böl
            if (current->size >= size + HEADER_SIZE + 4) {
                BlockHeader* new_block = (BlockHeader*)((uint8_t*)current + HEADER_SIZE + size);
                new_block->size = current->size - size - HEADER_SIZE;
                new_block->next = current->next;
                new_block->status = U_HEAP_FREE_BLOCK;
                new_block->alloc_reason = U_HEAP_ALLOC_REASON_KERNEL;

                current->size = size;
                current->next = new_block;
            }

            current->status = U_HEAP_ALLOCATED_BLOCK;
            current->alloc_reason = U_HEAP_ALLOC_REASON_KERNEL;

            return (void*)((uint8_t*)current + HEADER_SIZE);
        }

        prev = current;
        current = current->next;
    }

    // Uygun blok bulunamadı
    return NULL;
}

void u_free(void* ptr) {
    if (!ptr) return;

    BlockHeader* block = (BlockHeader*)((uint8_t*)ptr - HEADER_SIZE);
    block->status = U_HEAP_FREE_BLOCK;
    block->alloc_reason = U_HEAP_ALLOC_REASON_KERNEL;

    // 1. Sağ komşu ile birleşme
    if (block->next && block->next->status == U_HEAP_FREE_BLOCK) {
        BlockHeader* next = block->next;
        block->size += HEADER_SIZE + next->size;
        block->next = next->next;
    }

    // 2. Sol komşu ile birleşme (liste başından tarayarak)
    BlockHeader* current = free_list;
    while (current && current->next != block) {
        current = current->next;
    }

    if (current && current->status == U_HEAP_FREE_BLOCK) {
        current->size += HEADER_SIZE + block->size;
        current->next = block->next;
    }
}


size_t _u_free_heap() {
    size_t total = 0;
    BlockHeader* current = free_list;
    while (current) {
        if (current->status == U_HEAP_FREE_BLOCK)
            total += current->size;
        current = current->next;
    }
    return total;
}

size_t _u_allocated_block_count() {
    size_t total = 0;
    BlockHeader* current = free_list;
    while (current) {
        if (current->status != U_HEAP_FREE_BLOCK)
            total ++;
        current = current->next;
    }
    return total;
}

void memset(void* ptr, uint8_t value, size_t size){
    for(size_t i = 0; i < size; i++){
        ((uint8_t*)ptr)[i] = value;
    }
}

void memcpy(void* dst, void *src, size_t len){
    for(size_t i = 0; i < len; i++){
        ((uint8_t*)dst)[i] = ((uint8_t*)src)[i];
    }
}

int memcmp(void* src1, void *src2, size_t len){
    int result = 0;
    for(size_t i = 0; i < len; i++){
        if(((uint8_t*)src1)[i] != ((uint8_t*)src2)[i]){
            result++;
        }
    }
    return result;
}