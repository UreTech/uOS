#include "u_interrupt.h"

#include "u_ctypes.h"
#include "u_mmu.h"

// GIC-400 base addresses
#define GIC_BASE 0xFF840000
#define GICD_BASE (GIC_BASE + 0x1000)
#define GICC_BASE (GIC_BASE + 0x2000)

// Distributor registers
#define GICD_CTLR ((volatile uint32_t *)(GICD_BASE + 0x000))
#define GICD_ISENABLER ((volatile uint32_t *)(GICD_BASE + 0x100))
#define GICD_IPRIORITYR ((volatile uint32_t *)(GICD_BASE + 0x400))
#define GICD_ITARGETSR ((volatile uint32_t *)(GICD_BASE + 0x800))
#define GICD_ICFGR ((volatile uint32_t *)(GICD_BASE + 0xC00))

// CPU Interface registers
#define GICC_CTLR ((volatile uint32_t *)(GICC_BASE + 0x000))
#define GICC_PMR ((volatile uint32_t *)(GICC_BASE + 0x004))
#define GICC_IAR ((volatile uint32_t *)(GICC_BASE + 0x00C))
#define GICC_EOIR ((volatile uint32_t *)(GICC_BASE + 0x010))

// ARM Generic Timer interrupt numbers
#define TIMER_IRQ_PHYS 30 // Physical timer (CNTP)
#define TIMER_IRQ_VIRT 27 // Virtual timer (CNTV)

void gic_init(void)
{
	// Disable GIC
	*GICD_CTLR = 0;
	*GICC_CTLR = 0;

	// Set priority mask to allow all interrupts
	*GICC_PMR = 0xFF;

	// Enable GIC distributor
	*GICD_CTLR = 1;

	// Enable GIC CPU interface
	*GICC_CTLR = 1;
}

void gic_enable_timer_interrupt(void)
{
	uint32_t irq = TIMER_IRQ_PHYS; // IRQ 30

	// Enable interrupt in distributor
	uint32_t reg = irq / 32; // reg = 0
	uint32_t bit = irq % 32; // bit = 30
	GICD_ISENABLER[reg] = (1 << bit);

	// Set priority (0 = highest, 255 = lowest)
	reg = irq / 4;					// reg = 7
	uint32_t shift = (irq % 4) * 8; // shift = 16
	uint32_t val = GICD_IPRIORITYR[reg];
	val &= ~(0xFF << shift);
	val |= (0x80 << shift); // Priority 128 (medium)
	GICD_IPRIORITYR[reg] = val;

	// Set target CPU (CPU 0)
	reg = irq / 4;
	shift = (irq % 4) * 8;
	val = GICD_ITARGETSR[reg];
	val &= ~(0xFF << shift);
	val |= (0x01 << shift); // Target CPU 0
	GICD_ITARGETSR[reg] = val;
}

// Complete context save (DONT USE IT! ITS ALREADY IMPLEMENTED IN IRQ ENTRY)
__attribute__((naked, optnone)) void context_save(void *ctx)
{
	asm volatile(
		// x0 = pointer to context
		// [sp, #0] = original x0 value (saved by caller)

		// Save SP (add 16 because caller pushed x0)
		"add x9, sp, #16\n"
		"str x9, [x0, #0]\n"

		// Save PC from ELR_EL1
		"mrs x9, ELR_EL1\n"
		"str x9, [x0, #8]\n"

		// Save x0 from stack
		"ldr x11, [sp, #0]\n"
		"str x11, [x0, #16]\n"

		// Save x1-x30
		"str x1, [x0, #24]\n"
		"stp x2, x3, [x0, #32]\n"
		"stp x4, x5, [x0, #48]\n"
		"stp x6, x7, [x0, #64]\n"
		"stp x8, x9, [x0, #80]\n"
		"stp x10, x11, [x0, #96]\n"
		"stp x12, x13, [x0, #112]\n"
		"stp x14, x15, [x0, #128]\n"
		"stp x16, x17, [x0, #144]\n"
		"stp x18, x19, [x0, #160]\n"
		"stp x20, x21, [x0, #176]\n"
		"stp x22, x23, [x0, #192]\n"
		"stp x24, x25, [x0, #208]\n"
		"stp x26, x27, [x0, #224]\n"
		"stp x28, x29, [x0, #240]\n"
		"str x30, [x0, #256]\n"

		// Save SPSR_EL1
		"mrs x10, SPSR_EL1\n"
		"str x10, [x0, #264]\n"

		// Save FPCR/FPSR
		"mrs x10, FPCR\n"
		"str x10, [x0, #272]\n"
		"mrs x10, FPSR\n"
		"str x10, [x0, #280]\n"

		// Save SIMD registers - use str to avoid alignment issues
		"str q0, [x0, #288]\n"
		"str q1, [x0, #304]\n"
		"str q2, [x0, #320]\n"
		"str q3, [x0, #336]\n"
		"str q4, [x0, #352]\n"
		"str q5, [x0, #368]\n"
		"str q6, [x0, #384]\n"
		"str q7, [x0, #400]\n"
		"str q8, [x0, #416]\n"
		"str q9, [x0, #432]\n"
		"str q10, [x0, #448]\n"
		"str q11, [x0, #464]\n"
		"str q12, [x0, #480]\n"
		"str q13, [x0, #496]\n"
		"str q14, [x0, #512]\n"
		"str q15, [x0, #528]\n"
		"str q16, [x0, #544]\n"
		"str q17, [x0, #560]\n"
		"str q18, [x0, #576]\n"
		"str q19, [x0, #592]\n"
		"str q20, [x0, #608]\n"
		"str q21, [x0, #624]\n"
		"str q22, [x0, #640]\n"
		"str q23, [x0, #656]\n"
		"str q24, [x0, #672]\n"
		"str q25, [x0, #688]\n"
		"str q26, [x0, #704]\n"
		"str q27, [x0, #720]\n"
		"str q28, [x0, #736]\n"
		"str q29, [x0, #752]\n"
		"str q30, [x0, #768]\n"
		"str q31, [x0, #784]\n"

		// Save TPIDR_EL0
		"mrs x10, TPIDR_EL0\n"
		"str x10, [x0, #800]\n"

		"ret\n");
}

// DO NOT CHANGE THIS!
#define IRQ_STACK_SIZE 8192

void u_thread_irq_handler_(void);

// Exception vector table entry (assembly wrapper)
__attribute__((naked)) void irq_vector_entry(void)
{
	asm volatile(
		// Save x0-x1 temporarily
		"stp x0, x1, [sp, #-16]!\n"

		// Get current thread context pointer
		"adrp x0, current_thread\n"
		"add x0, x0, :lo12:current_thread\n"
		"ldr w0, [x0]\n"

		"adrp x1, threads\n"
		"add x1, x1, :lo12:threads\n"
		"ldr x1, [x1]\n"

		"mov x2, #864\n"
		"madd x0, x0, x2, x1\n"
		"add x0, x0, #32\n"

		// Save SP
		"add x9, sp, #16\n"
		"str x9, [x0, #0]\n"

		// Save PC from ELR_EL1
		"mrs x9, ELR_EL1\n"
		"str x9, [x0, #8]\n"

		// Save x2-x30
		"stp x2, x3, [x0, #32]\n"
		"stp x4, x5, [x0, #48]\n"
		"stp x6, x7, [x0, #64]\n"
		"stp x8, x9, [x0, #80]\n"
		"stp x10, x11, [x0, #96]\n"
		"stp x12, x13, [x0, #112]\n"
		"stp x14, x15, [x0, #128]\n"
		"stp x16, x17, [x0, #144]\n"
		"stp x18, x19, [x0, #160]\n"
		"stp x20, x21, [x0, #176]\n"
		"stp x22, x23, [x0, #192]\n"
		"stp x24, x25, [x0, #208]\n"
		"stp x26, x27, [x0, #224]\n"
		"stp x28, x29, [x0, #240]\n"
		"str x30, [x0, #256]\n"

		// Save original x0, x1
		"ldp x9, x10, [sp]\n"
		"stp x9, x10, [x0, #16]\n"

		// Save SPSR_EL1
		"mrs x10, SPSR_EL1\n"
		"str x10, [x0, #264]\n"

		// Save FPCR/FPSR
		"mrs x10, FPCR\n"
		"str x10, [x0, #272]\n"
		"mrs x10, FPSR\n"
		"str x10, [x0, #280]\n"

		// **FIX: Use individual str instead of stp for SIMD**
		"str q0, [x0, #288]\n"
		"str q1, [x0, #304]\n"
		"str q2, [x0, #320]\n"
		"str q3, [x0, #336]\n"
		"str q4, [x0, #352]\n"
		"str q5, [x0, #368]\n"
		"str q6, [x0, #384]\n"
		"str q7, [x0, #400]\n"
		"str q8, [x0, #416]\n"
		"str q9, [x0, #432]\n"
		"str q10, [x0, #448]\n"
		"str q11, [x0, #464]\n"
		"str q12, [x0, #480]\n"
		"str q13, [x0, #496]\n"
		"str q14, [x0, #512]\n"
		"str q15, [x0, #528]\n"
		"str q16, [x0, #544]\n"
		"str q17, [x0, #560]\n"
		"str q18, [x0, #576]\n"
		"str q19, [x0, #592]\n"
		"str q20, [x0, #608]\n"
		"str q21, [x0, #624]\n"
		"str q22, [x0, #640]\n"
		"str q23, [x0, #656]\n"
		"str q24, [x0, #672]\n"
		"str q25, [x0, #688]\n"
		"str q26, [x0, #704]\n"
		"str q27, [x0, #720]\n"
		"str q28, [x0, #736]\n"
		"str q29, [x0, #752]\n"
		"str q30, [x0, #768]\n"
		"str q31, [x0, #784]\n"

		// Save TPIDR_EL1
		"mrs x10, TPIDR_EL1\n"
		"str x10, [x0, #800]\n"

		// Restore stack pointer
		"add sp, sp, #16\n"

		// Switch to IRQ stack
		"adrp x0, irq_stack\n"
		"add x0, x0, :lo12:irq_stack\n"
		"ldr x0, [x0]\n"
		"add x0, x0, #8192\n"
		"and x0, x0, #-16\n"
		"mov sp, x0\n"

		// Call C IRQ handler
		"bl u_thread_irq_handler_\n"

		"b .\n");
}

// unused
__attribute__((aligned(0x800))) void (*exception_vectors[16])(void) = {
	[0] = 0,				// Synchronous EL1t
	[1] = 0,				// IRQ EL1t
	[2] = 0,				// FIQ EL1t
	[3] = 0,				// SError EL1t
	[4] = 0,				// Synchronous EL1h
	[5] = irq_vector_entry, // IRQ EL1h ← THIS IS WHAT WE NEED!
	[6] = 0,				// FIQ EL1h
	[7] = 0,				// SError EL1h
	[8] = 0,				// Synchronous EL0 (64-bit)
	[9] = 0,				// IRQ EL0 (64-bit)
	[10] = 0,				// FIQ EL0 (64-bit)
	[11] = 0,				// SError EL0 (64-bit)
	[12] = 0,				// Synchronous EL0 (32-bit)
	[13] = 0,				// IRQ EL0 (32-bit)
	[14] = 0,				// FIQ EL0 (32-bit)
	[15] = 0,				// SError EL0 (32-bit)
};

void install_exception_vectors(void)
{
	uint64_t addr = (uint64_t)exception_vectors;
	asm volatile("msr vbar_el1, %0" ::"r"(addr));
}

void u_timer_interrupt_init_()
{
	//install_exception_vectors(); boot.S does it

	// STEP 2: Initialize GIC
	gic_init();

	// STEP 3: Enable timer interrupt in GIC
	gic_enable_timer_interrupt();

	/*
	// STEP 4: Set timer value (1 second)
	uint64_t freq = u_arm_read_cntfrq(); // Usually 54,000,000 Hz
	u_arm_write_cntp_tval_el0(freq);	 // 1 second

	// STEP 5: Enable timer and CPU IRQ
	u_arm_enable_cntp_irq();
	*/
}

unsigned long long u_arm_read_cntfrq(void)
{
	unsigned long long val;
	asm volatile("mrs %0, cntfrq_el0"
				 : "=r"(val));
	return val;
}

void u_arm_enable_cntp_irq(void)
{
	uint64_t val;

	// Enable CNTP timer
	asm volatile("mrs %0, CNTP_CTL_EL0"
				 : "=r"(val));
	val |= 1;		  // ENABLE = 1
	val &= ~(1 << 1); // IMASK = 0 (unmask interrupt)
	asm volatile("msr CNTP_CTL_EL0, %0" ::"r"(val));

	// Enable IRQ in CPU
	asm volatile("msr DAIFClr, #2");
}

void u_arm_disable_cntp_irq(void)
{
	uint64_t val;

	// Disable CNTP timer
	asm volatile("mrs %0, CNTP_CTL_EL0"
				 : "=r"(val));
	val &= ~1; // ENABLE = 0
	asm volatile("msr CNTP_CTL_EL0, %0" ::"r"(val));

	// Disable IRQ in CPU
	asm volatile("msr DAIFSet, #2");
}

void u_arm_instant_cntp_irq(void)
{
	u_arm_write_cntp_tval_el0(0);
}

void u_arm_write_cntp_tval_el0(uint64_t val)
{
	asm volatile("msr CNTP_TVAL_EL0, %0" ::"r"(val));
}

#include "u_uart.h"

void vbar_set(void *vtable)
{
	// VBAR register set
	uint64_t addr = (uint64_t)vtable;

	uart_print("vbar_el1: ");
	uart_print_hex64(addr);
	uart_print("\n");

	asm volatile("msr vbar_el1, %0" ::"r"(addr));
	asm volatile("isb");
}