#pragma once

#include <u_ctypes.h>
#include <u_uart.h>

// UreTech Memory Page Allocator/Manager

#define ALIGN_UP(x, align) (((x) + (align)-1) & ~((align)-1))
#define ALIGN_DOWN(x, align) ((x) & ~((align)-1))

#define UMP_PAGE_SIZE 0x1000
#define UMP_PAGE16_SIZE 0x4000
#define UMP_PAGE64_SIZE 0x10000

typedef uint8_t onepage[UMP_PAGE_SIZE];
typedef uint8_t onepage16[UMP_PAGE16_SIZE];
typedef uint8_t onepage64[UMP_PAGE64_SIZE];

// memory
extern char __heap_base[];
extern char __heap_end[];
static void *mem = (void *)&__heap_base;
static void *mem_end = (void *)&__heap_end;

// video memory
extern char __vheap_base[];
extern char __vheap_end[];
static void *vmem = (void *)&__vheap_base;
static void *vmem_end = (void *)&__vheap_end;

// Page Ownership Flags
#define UMP_FLAG_PINNED_BIT ONEBIT(0)				 // This page is can not be moved
#define UMP_FLAG_KERNEL_BIT ONEBIT(1)				 // This page is ONLY kernel use
#define UMP_FLAG_PP_RW_BIT ONEBIT(3)				 // This page is Read/Write for this owner
#define UMP_FLAG_PP_X_BIT ONEBIT(4)					 // This page is Executable for this owner
#define UMP_FLAG_UHP_DISABLE_NEW_ALLOC_BIT ONEBIT(5) // This page is cant be used by UHP as allocatable page (for restricted pages)
#define UMP_FLAG_IN_SWAP_BIT ONEBIT(6)				 // This page is currently not in memory
#define UMP_FLAG_EMPTY_BIT ONEBIT(7)				 // This ownership entry is empty(not allocated)

// Note: if UMP_FLAG_PP_RW_BIT is not set, it assumes as RO(Read Only)
// Note: Pages flagged with UMP_FLAG_KERNEL_BIT is inaccessable to user space
// Note: if UMP_FLAG_KERNEL_BIT flag is set, PP(Page Permission) bits are assumed as kernel's permission
// Note: if UMP_FLAG_PINNED_BIT is set, kernel cant move that pages physical location

// Memory Region information struct (in first page of region)
// Region Map (Simplified)
// | IID |    Address   |          Name          |           Size           |
// |  0  |  First page  |    [MRegion Header]    |          1 page          |
// |  1  | After IID[0] |    [Free Page Table]   |  calculated in runtime   |
// |  2  | After IID[1] |      [Data Pages]      | remaining part of memory |
typedef struct
{
	uint64_t total_page_count;
	uint64_t FPT_page_count; // page count of Free Page Table
	uint64_t DP_page_count;	 // page count of Data Pages
	uint64_t allocatedPageCount;
	
	// tables
	uint8_t *FPT;

	uint8_t *DP_start;

	uint8_t *_raw_first_page_;
} __attribute__((aligned(16))) UMP_MREGION_HEADER;

void ump_allocator_init_mem();

void ump_allocator_init_vmem();

// this functions are DANGEROUS so DO NOT USE IF YOU DONT KNOW HOW TO USE!

#define UMP_FPT_FREE 0
#define UMP_FPT_ALLOCATED 1

void ump_FPT_set_page_status(uint64_t page, uint8_t status, UMP_MREGION_HEADER *mmrh);
void ump_FPT_set_range_page_status(uint64_t page, uint64_t page_count, uint8_t status, UMP_MREGION_HEADER *mmrh);

// external public functions

// allocate page(s) with physical address pointer (tries to allocate physicaly continuous pages)
void *ump_palloc(uint64_t page_count);
void *ump_vpalloc(uint64_t page_count);

// free pages
//  WARNING! This system doesnt checks double free! So becareful while using this command.
// Note: resource manager should check double free and page ownership
void ump_pfree(void *firstPage, uint64_t page_count);
void ump_vpfree(void *firstPage, uint64_t page_count);

// get allocated page count
// WARNING! This function may return wrong size if resource managment is buggy
uint64_t ump_allocated_page_count();
uint64_t ump_allocated_vpage_count();
