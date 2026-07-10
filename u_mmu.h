#ifndef U_MMU_H
#define U_MMU_H

#include <u_ctypes.h>

// common addresses
#define UOS_TTBR0_MIN (0x0ULL)
#define UOS_TTBR0_MAX (0x0000FFFFFFFFFFFFULL)

#define UOS_TTBR1_MIN (0xFFFF000000000000ULL)
#define UOS_TTBR1_MAX (0xFFFFFFFFFFFFFFFFULL)

// fix memory locations
extern char __kernel_text_start__[];
static void *_kernel_text_start_ = (void *)&__kernel_text_start__;
extern char __kernel_text_end__[];
static void *_kernel_text_end_ = (void *)&__kernel_text_end__;

extern char __kernel_rodata_start__[];
static void *_kernel_rodata_start_ = (void *)&__kernel_rodata_start__;
extern char __kernel_rodata_end__[];
static void *_kernel_rodata_end_ = (void *)&__kernel_rodata_end__;

extern char __kernel_data_start__[];
static void *_kernel_data_start_ = (void *)&__kernel_data_start__;
extern char __kernel_data_end__[];
static void *_kernel_data_end_ = (void *)&__kernel_data_end__;

extern char __bss_start[];
static void *_kernel_bss_start_ = (void *)&__bss_start;
extern char __bss_end[];
static void *_kernel_bss_end_ = (void *)&__bss_end;

extern char __stack_start[];
static void *_kernel_stack_start_pa_ptr_ = (void *)&__stack_start;
extern char __stack_end[];
static void *_kernel_stack_end_pa_ptr_ = (void *)&__stack_end;

// page table entry flags (no need to use them it already handled in functions)
#define PTE_VALID ONEBIT(0)
#define PTE_PAGE ONEBIT(1)

// atribute index
#define PTE_ATTRIDX(x) ((uint64_t)(x) << 2)

#define PTE_ATTRIDX_DEVICE PTE_ATTRIDX(0)
#define PTE_ATTRIDX_MEMORY PTE_ATTRIDX(1)

// access permission flags
#define PTE_AP0_EL0_ACCESS ONEBIT(6)
#define PTE_AP0_EL0_NO_ACCESS 0ULL
#define PTE_AP1_RW 0ULL
#define PTE_AP1_RO ONEBIT(7)

#define PTE_SH_NONE (0ULL << 8)
#define PTE_SH_OUTER (2ULL << 8)
#define PTE_SH_INNER (3ULL << 8)

#define PTE_AF (1ULL << 10)
#define PTE_NG (1ULL << 11) // not used (we dont have user space for now)

#define PTE_PXN (1ULL << 53) // EL1+ never exec
#define PTE_UXN (1ULL << 54) // EL0 never exec

// preset flags
#define PTE_PRESET_CORE (PTE_SH_INNER | PTE_AF)

#define PTE_PRESET_KERNEL_EXECUTABLE (PTE_PRESET_CORE | PTE_ATTRIDX_MEMORY | PTE_AP0_EL0_NO_ACCESS | PTE_AP1_RO | PTE_UXN)

#define PTE_PRESET_KERNEL_RWX (PTE_PRESET_CORE | PTE_ATTRIDX_MEMORY | PTE_AP0_EL0_NO_ACCESS | PTE_AP1_RW | PTE_UXN)

#define PTE_PRESET_KERNEL_DATA (PTE_PRESET_CORE | PTE_ATTRIDX_MEMORY | PTE_AP0_EL0_NO_ACCESS | PTE_AP1_RW | PTE_UXN | PTE_PXN)

#define PTE_PRESET_KERNEL_RODATA (PTE_PRESET_CORE | PTE_ATTRIDX_MEMORY | PTE_AP0_EL0_NO_ACCESS | PTE_AP1_RO | PTE_UXN | PTE_PXN)

#define PTE_PRESET_KERNEL_MMIO (PTE_PRESET_CORE | PTE_ATTRIDX_DEVICE | PTE_AP0_EL0_NO_ACCESS | PTE_AP1_RW | PTE_UXN | PTE_PXN)

#define PTE_ADDR_MASK 0x0000FFFFFFFFF000ULL

// MMU Context
typedef struct PageTableNode
{
	uint64_t *table;
	struct PageTableNode *next;
} PageTableNode;

typedef struct
{
	uint64_t *L0_ttbr0;
	uint64_t *L0_ttbr1;
	PageTableNode *allocated;
} mmu_context_t;

void mmu_setup();

mmu_context_t *mmu_create_context();
void mmu_destroy_context(mmu_context_t *ctx); // not implemented yet

// void mmu_map(uint64_t va, uint64_t pa, uint64_t flags, mmu_context_t *ctx);
// void mmu_unmap(uint64_t va, mmu_context_t *ctx);

// this functions are not completely optimised
// should be revisioned in the future
void mmu_map_range(uint64_t va, uint64_t pa, uint64_t length, uint64_t flags, mmu_context_t *ctx);
void mmu_unmap_range(uint64_t va, uint64_t length, mmu_context_t *ctx);

void mmu_enable();
void mmu_disable();

void mmu_barrier();
void mmu_flush_tlb();

void mmu_set_ttbr0(void *root);
void mmu_set_ttbr1(void *root);
void mmu_switch_ttbr(mmu_context_t *ctx);

// debug
void _mmu_dump_L0_(uint64_t *L0);

void mmu_fart(uint64_t fart_value); // nope

#endif