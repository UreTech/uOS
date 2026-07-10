.section .text.boot, "ax"
    .cpu cortex-a72
    .align 3

    .extern __stack_top
    .extern __bss_start
    .extern __bss_size
    .global _start
    .global el1_sync_handler
    .global el1_vectors
    .global high_va_el1_vectors

_start:
    /* Core 0 only */
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbz     x1, 2f
1:  wfe
    b       1b
2:

    /* Set EL2 stack */
    ldr     x0, =__stack_top
    bic     x0, x0, #0xF
    mov     sp, x0

    /* Zero BSS */
    ldr     x1, =__bss_start
    ldr     x2, =__bss_size
    cbz     x2, bss_done
bss_loop:
    str     xzr, [x1], #8
    subs    x2, x2, #8
    b.gt    bss_loop
bss_done:

    /* Disable coprocessor traps */
    msr     CPTR_EL2, xzr
    
    /* Enable AArch64 in EL1 */
    mov     x0, #(1 << 31)
    msr     HCR_EL2, x0

    /* Set EL1 execution state */
    mov     x0, #0x3C5
    msr     SPSR_EL2, x0

    /* Set EL1 stack pointer */
    ldr     x0, =__stack0_top
    bic     x0, x0, #0xF
    msr     SP_EL1, x0

    /* Set EL1 vector table */
    ldr     x0, =el1_vectors
    msr     VBAR_EL1, x0

    /* Set EL1 entry point */
    adr     x0, el1_entry
    msr     ELR_EL2, x0

    /* Barrier before transition */
    dsb     sy
    isb

    eret

    .align 3
el1_entry:

	/* Clear TPIDR */
	bl clear_tpidr
	
    /* Enable FP/SIMD FIRST */
    mov     x0, #(0x3 << 20)
    msr     CPACR_EL1, x0
    isb

    /* Disable MMU and caches EARLY - before they cause issues */
    mrs     x0, SCTLR_EL1
    bic     x0, x0, #(1 << 0)       // MMU
    bic	 x0, x0, #(1 << 1) // unaligned access
    bic     x0, x0, #(1 << 2)       // Data cache
    bic     x0, x0, #(1 << 12)      // Instruction cache
    msr     SCTLR_EL1, x0
    isb

    /* Full cache invalidation sequence */
    ic      iallu                   // Invalidate all instruction caches
    dsb     sy
    
    /* Also invalidate data cache to PoC (Point of Coherency) */
    mov     x0, #0
    msr     csselr_el1, x0          // Select L1 data cache
    isb
    mrs     x4, ccsidr_el1          // Read cache size
    
    /* Simple full cache flush - covers all cache levels */
    dsb     sy
    isb

    /* NOW it's safe to use the stack */
    mov     x29, sp                 // Set frame pointer
    
    /* DON'T zero x30 - set it to a safe return address */
    adr     x30, hang              // If main returns, go to hang

    /* Call main */
    bl      main

hang:
    wfe
    b       hang

/* Exception vectors */
.align 11
el1_vectors:

// 1 EL1 sp EL1 Sync
.align 7
b el1_sync_handler
.space 128-4

// 2 EL1 sp EL1 IRQ
.align 7
b el1_irq_handler
.space 128-4

// 3 EL1 sp EL1 FIQ
.align 7
b el1_fiq_handler
.space 128-4

// 4 EL1 sp EL1 SError
.align 7
b el1_serror_handler
.space 128-4

// 5 EL1 sp EL0 Sync
.align 7
b el1_sync_handler
.space 128-4

// 6 EL1 sp EL0 IRQ
.align 7
b el1_irq_handler
.space 128-4

// 7 EL1 sp EL0 FIQ
.align 7
b el1_fiq_handler
.space 128-4

// 8 EL1 sp EL0 SError
.align 7
b el1_serror_handler
.space 128-4

// 9 EL0 sp EL1 Sync
.align 7
b el1_sync_handler
.space 128-4

// 10 EL0 sp EL1 IRQ
.align 7
b el1_irq_handler
.space 128-4

// 11 EL0 sp EL1 FIQ
.align 7
b el1_fiq_handler
.space 128-4

// 12 EL0 sp EL1 SError
.align 7
b el1_serror_handler
.space 128-4

// 13 EL0 sp EL0 Sync
.align 7
b el1_sync_handler
.space 128-4

// 14 EL0 sp EL0 IRQ
.align 7
b el1_irq_handler
.space 128-4

// 15 EL0 sp EL0 FIQ
.align 7
b el1_fiq_handler
.space 128-4

// 16 EL0 sp EL0 SError
.align 7
b el1_serror_handler
.space 128-4

.set HIGH_OFFSET, 0xFFFF000000000000

    .section .vectors_high, "ax"
    .align 11
high_va_el1_vectors:

// ========== Current EL with SP0 ==========
    .align 7
el1t_sync_vector:
    b el1_sync_handler
    .align 7
el1t_irq_vector:
    b el1_irq_handler
    .align 7
el1t_fiq_vector:
    b el1_fiq_handler
    .align 7
el1t_serror_vector:
    b el1_serror_handler

// ========== Current EL with SPx ==========
    .align 7
el1h_sync_vector:
    b el1_sync_handler
    .align 7
el1h_irq_vector:
    b el1_irq_handler
    .align 7
el1h_fiq_vector:
    b el1_fiq_handler
    .align 7
el1h_serror_vector:
    b el1_serror_handler

// ========== Lower EL (AArch64) ==========
    .align 7
el0_sync_vector:
    b el1_sync_handler
    .align 7
el0_irq_vector:
    b el1_irq_handler
    .align 7
el0_fiq_vector:
    b el1_fiq_handler
    .align 7
el0_serror_vector:
    b el1_serror_handler

// ========== Lower EL (AArch32) ==========
    .align 7
el0_32_sync_vector:
    b el1_sync_handler
    .align 7
el0_32_irq_vector:
    b el1_irq_handler
    .align 7
el0_32_fiq_vector:
    b el1_fiq_handler
    .align 7
el0_32_serror_vector:
    b el1_serror_handler

clear_tpidr:
    mov     x0, #0
    msr     tpidr_el1, x0
    msr     tpidr_el0, x0
    
    isb
    
    ret
    
el1_sync_handler:
	stp x29, x30, [sp, #-16]!
	stp x0, x1, [sp, #-16]!
	stp x2, x3, [sp, #-16]!
	
	mrs x0, ESR_EL1
	mrs x1, FAR_EL1
	mrs x2, ELR_EL1
	mrs x3, SPSR_EL1
	
	bl  print_exception_info
	
	ldp x2, x3, [sp], #16
	ldp x0, x1, [sp], #16
	ldp x29, x30, [sp], #16
 1:  b   1b

el1_irq_handler:
	b irq_vector_entry
el1_fiq_handler:
el1_serror_handler:
    b       el1_sync_handler