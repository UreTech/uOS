#include "u_thread.h"

// DO NOT CHANGE THIS!
#define IRQ_STACK_SIZE 8192

typedef uint32_t u_thread_owner_;
typedef uint32_t u_thread_flags_;

// Complete thread context structure
typedef struct
{
	// Stack and PC
	uint64_t sp; // offset 0
	uint64_t pc; // offset 8

	// General purpose registers (x0-x30)
	uint64_t x0;  // offset 16
	uint64_t x1;  // offset 24
	uint64_t x2;  // offset 32
	uint64_t x3;  // offset 40
	uint64_t x4;  // offset 48
	uint64_t x5;  // offset 56
	uint64_t x6;  // offset 64
	uint64_t x7;  // offset 72
	uint64_t x8;  // offset 80
	uint64_t x9;  // offset 88
	uint64_t x10; // offset 96
	uint64_t x11; // offset 104
	uint64_t x12; // offset 112
	uint64_t x13; // offset 120
	uint64_t x14; // offset 128
	uint64_t x15; // offset 136
	uint64_t x16; // offset 144
	uint64_t x17; // offset 152
	uint64_t x18; // offset 160
	uint64_t x19; // offset 168
	uint64_t x20; // offset 176
	uint64_t x21; // offset 184
	uint64_t x22; // offset 192
	uint64_t x23; // offset 200
	uint64_t x24; // offset 208
	uint64_t x25; // offset 216
	uint64_t x26; // offset 224
	uint64_t x27; // offset 232
	uint64_t x28; // offset 240
	uint64_t x29; // offset 248 (FP)
	uint64_t x30; // offset 256 (LR)

	// Processor state
	uint64_t spsr_el1; // offset 264

	// Floating-point control/status
	uint64_t fpcr; // offset 272
	uint64_t fpsr; // offset 280

	// SIMD/FP registers (128-bit each, stored as pairs of uint64_t)
	uint64_t v0[2];	 // offset 288
	uint64_t v1[2];	 // offset 304
	uint64_t v2[2];	 // offset 320
	uint64_t v3[2];	 // offset 336
	uint64_t v4[2];	 // offset 352
	uint64_t v5[2];	 // offset 368
	uint64_t v6[2];	 // offset 384
	uint64_t v7[2];	 // offset 400
	uint64_t v8[2];	 // offset 416
	uint64_t v9[2];	 // offset 432
	uint64_t v10[2]; // offset 448
	uint64_t v11[2]; // offset 464
	uint64_t v12[2]; // offset 480
	uint64_t v13[2]; // offset 496
	uint64_t v14[2]; // offset 512
	uint64_t v15[2]; // offset 528
	uint64_t v16[2]; // offset 544
	uint64_t v17[2]; // offset 560
	uint64_t v18[2]; // offset 576
	uint64_t v19[2]; // offset 592
	uint64_t v20[2]; // offset 608
	uint64_t v21[2]; // offset 624
	uint64_t v22[2]; // offset 640
	uint64_t v23[2]; // offset 656
	uint64_t v24[2]; // offset 672
	uint64_t v25[2]; // offset 688
	uint64_t v26[2]; // offset 704
	uint64_t v27[2]; // offset 720
	uint64_t v28[2]; // offset 736
	uint64_t v29[2]; // offset 752
	uint64_t v30[2]; // offset 768
	uint64_t v31[2]; // offset 784

	// Thread-local storage pointer
	uint64_t tpidr_el0; // offset 800

	/// mmu registers are set in irq dynamically

	// Metadata (not saved/restored)
	int state;		  // offset 808
	void *stack_base; // offset 816
} u_thread_context_;

typedef struct __attribute__((aligned(16)))
{
	u_thread_flags_ flags;	   // 4 bytes
	uint32_t _pad1;			   // 4 bytes explicit padding
	uint64_t priority;		   // 8 bytes
	uint64_t score;			   // 8 bytes
	uint64_t __padding;		   // 8 bytes
	u_thread_context_ context; // 824 bytes, starts at offset 32
	// Note: mmu context not saved in interrupt becasue its pointers are fixed
	mmu_context_t *mmu_ctx; // mmu context
} u_thread_;

u_thread_ *threads = NULL;

// *** UTIL ***

// returns next shecduled thread id ( retruns -1 if nothing found)
uint32_t u_thread_shecdule_next_()
{
	uint64_t highest_id = -1;
	uint64_t highest_score = 0;

	// skipping first(kernel) second(dummy context)
	for (int i = 2; i < UTH_MAX_THREADS; i++)
	{
		if (threads[i].flags & UTH_FLAG_READY_BIT)
		{
			if (threads[i].score > highest_score)
			{
				highest_score = threads[i].score;
				highest_id = i;
			}
			// update score
			threads[i].score += threads[i].priority;
		}
	}

	if (highest_id != -1)
	{
		threads[highest_id].score = 0; // Reset score
	}

	return highest_id;
}

// Complete context load
__attribute__((naked, optnone)) void context_load(u_thread_context_ *ctx)
{
	asm volatile(
		// Load SIMD registers - USE INDIVIDUAL ldr INSTEAD OF ldp
		"ldr q0, [x0, #288]\n"
		"ldr q1, [x0, #304]\n"
		"ldr q2, [x0, #320]\n"
		"ldr q3, [x0, #336]\n"
		"ldr q4, [x0, #352]\n"
		"ldr q5, [x0, #368]\n"
		"ldr q6, [x0, #384]\n"
		"ldr q7, [x0, #400]\n"
		"ldr q8, [x0, #416]\n"
		"ldr q9, [x0, #432]\n"
		"ldr q10, [x0, #448]\n"
		"ldr q11, [x0, #464]\n"
		"ldr q12, [x0, #480]\n"
		"ldr q13, [x0, #496]\n"
		"ldr q14, [x0, #512]\n"
		"ldr q15, [x0, #528]\n"
		"ldr q16, [x0, #544]\n"
		"ldr q17, [x0, #560]\n"
		"ldr q18, [x0, #576]\n"
		"ldr q19, [x0, #592]\n"
		"ldr q20, [x0, #608]\n"
		"ldr q21, [x0, #624]\n"
		"ldr q22, [x0, #640]\n"
		"ldr q23, [x0, #656]\n"
		"ldr q24, [x0, #672]\n"
		"ldr q25, [x0, #688]\n"
		"ldr q26, [x0, #704]\n"
		"ldr q27, [x0, #720]\n"
		"ldr q28, [x0, #736]\n"
		"ldr q29, [x0, #752]\n"
		"ldr q30, [x0, #768]\n"
		"ldr q31, [x0, #784]\n"

		// Load FPCR/FPSR
		"ldr x9, [x0, #272]\n"
		"msr FPCR, x9\n"
		"ldr x9, [x0, #280]\n"
		"msr FPSR, x9\n"

		// Load TPIDR
		"ldr x9, [x0, #800]\n"
		"msr TPIDR_EL0, x9\n"
		"msr TPIDR_EL1, x9\n"

		// Load general purpose registers x1-x30
		"ldr x1, [x0, #24]\n"
		"ldp x2, x3, [x0, #32]\n"
		"ldp x4, x5, [x0, #48]\n"
		"ldp x6, x7, [x0, #64]\n"
		"ldp x8, x9, [x0, #80]\n"
		"ldp x10, x11, [x0, #96]\n"
		"ldp x12, x13, [x0, #112]\n"
		"ldp x14, x15, [x0, #128]\n"
		"ldp x16, x17, [x0, #144]\n"
		"ldp x18, x19, [x0, #160]\n"
		"ldp x20, x21, [x0, #176]\n"
		"ldp x22, x23, [x0, #192]\n"
		"ldp x24, x25, [x0, #208]\n"
		"ldp x26, x27, [x0, #224]\n"
		"ldp x28, x29, [x0, #240]\n"
		"ldr x30, [x0, #256]\n"

		// Load SPSR_EL1
		"ldr x9, [x0, #264]\n"
		"msr SPSR_EL1, x9\n"

		// Load PC to ELR_EL1
		"ldr x9, [x0, #8]\n"
		"msr ELR_EL1, x9\n"

		// Load SP
		"ldr x9, [x0, #0]\n"
		"mov sp, x9\n"

		// Save context pointer temporarily and load original x0
		"mov x9, x0\n"		  // Save context pointer
		"ldr x0, [x9, #16]\n" // Load original x0 value NOW

		// Return via exception return
		"eret\n");
}

// active context owner
uint64_t current_thread = 0;

void _u_thread_end_handler()
{
	// stop irq because destroying proccess must be not interrupted
	u_arm_disable_cntp_irq();

	// destroy thread
	u_thread_destroy(current_thread);

	// set dummy for not crushing when its interrupts
	current_thread = UOS_DUMMY_THREAD_ID;

	// enable and instant irq
	u_arm_enable_cntp_irq();
	u_arm_instant_cntp_irq();

	// wait if some how goes here
	while (1)
	{
		asm volatile("wfi");
	}

	// must not reach here
}

uint32_t u_thread_destroy(uint64_t thread_id)
{
	if (!(threads[thread_id].flags & UTH_FLAG_NULL_BIT))
	{
		// set not ready first
		threads[thread_id].flags &= ~UTH_FLAG_READY_BIT;

		// free mmu context if exists (must exsist :sob:)
		if (threads[thread_id].mmu_ctx != NULL)
		{
			// !!!!NOT WORKIN YET !!!!
			// mmu_free_context(threads[thread_id].mmu_ctx);

			threads[thread_id].mmu_ctx = NULL;
		}

		// set as null now
		threads[thread_id].flags = UTH_FLAG_NULL_BIT;

		return UTH_SUCCESS;
	}

	return UTH_FAIL;
}

void u_create_context_(u_thread_context_ *ctx, void (*entry_point)(void), void *stack, size_t stack_size)
{
	// Zero out the context
	memset(ctx, 0, sizeof(u_thread_context_));

	// Set stack pointer (stack grows downward)
	ctx->stack_base = stack;
	ctx->sp = (uint64_t)stack + stack_size;

	// Align stack to 16 bytes (ARM64 requirement)
	ctx->sp &= ~0xFULL;

	// Set program counter to entry point
	ctx->pc = (uint64_t)entry_point;
	ctx->x30 = (uint64_t)_u_thread_end_handler; // Link register to _u_thread_end_handler for handling return state

	uart_print("Thread Entry: ");
	uart_print_hex64(ctx->pc);
	uart_print("\n");

	// Set processor state (EL1, interrupts enabled)
	ctx->spsr_el1 = 0x305;
}

uint32_t u_thread_create(void (*entry)(void), uint64_t priority, int is_el1)
{
	// finding empty slot
	// skipping first(kernel) second(dummy context)
	for (int i = 2; i < UTH_MAX_THREADS; i++)
	{
		if (threads[i].flags & UTH_FLAG_NULL_BIT)
		{
			// found!

			// set null to false first
			threads[i].flags = 0; // bit 0 is 0

			// allocate stack if not available
			void *stack_base = threads[i].context.stack_base;
			if (stack_base == NULL)
			{
				stack_base = kmalloc(16 * 1024);
			}

			void *stack = (void *)((((uint64_t)stack_base) + 0xF) & ~0xF);

			size_t alignment_offset = (uint64_t)stack - (uint64_t)stack_base;
			size_t usable_size = (16 * 1024) - alignment_offset;

			// create context
			u_create_context_(&threads[i].context, entry, stack, usable_size - 16);

			// create mmu context if not exsists
			if (threads[i].mmu_ctx == NULL)
			{
				// threads[i].mmu_ctx = mmu_create_context();
			}

			// set priority
			threads[i].priority = priority;

			if (is_el1)
			{
				// set ready and el1 flag at the end
				threads[i].flags |= UTH_FLAG_READY_BIT | UTH_FLAG_EL1_BIT;
			}
			else
			{
				// set ready flag at the end
				threads[i].flags |= UTH_FLAG_READY_BIT;
			}
			// return id
			return i;
		}
	}

	return -1; // fail
}

// *** UTIL END ***

void *irq_stack = NULL;

void u_thread_initsys_()
{
	u_timer_interrupt_init_();

	threads = (u_thread_ *)kmalloc(UTH_MAX_THREADS * sizeof(u_thread_));

	irq_stack = kmalloc(IRQ_STACK_SIZE);

	// create only mmu context for kernel
	threads[UOS_KERNEL_THREAD_ID].mmu_ctx = mmu_create_context();

	for (int i = 0; i < UTH_MAX_THREADS; i++)
	{
		threads[i].flags = UTH_FLAG_NULL_BIT; // set all null
	}
}

uint32_t job_done = 0;
uint32_t total_job = 0;

void kernel_idle_loop(void)
{
	while (job_done < total_job)
	{
		udb();
		delay_ms(0);
		//asm volatile("wfi");
		delay_ms(0);
		udb();
	}
}

void u_thread_run_threads_(uint32_t ttr)
{
	job_done = 0;
	total_job = ttr;

	//	current_thread = UOS_KERNEL_THREAD_ID;

	// set up timer interrupt
	u_arm_enable_cntp_irq();
	u_arm_instant_cntp_irq();

	// wait threads...
	kernel_idle_loop();

	// be sure irq is disabled
	u_arm_disable_cntp_irq();
}

#define GIC_BASE 0xFF840000
#define GICC_BASE (GIC_BASE + 0x2000)
#define GICC_IAR ((volatile uint32_t *)(GICC_BASE + 0x00C))
#define GICC_EOIR ((volatile uint32_t *)(GICC_BASE + 0x010))

__attribute__((optnone)) void u_thread_irq_handler_()
{
	// this page must be mapped correctly or we already must be at kernel thread page table so we must not blow up here

	// first save current (entry does)

	uint32_t irq_id = *GICC_IAR;
	*GICC_EOIR = irq_id;

	if (job_done < total_job)
	{
		// get next
		uint32_t next = u_thread_shecdule_next_();

		if (next == -1)
		{
			// no job. break
			job_done = total_job;

			// disable irq
			u_arm_disable_cntp_irq();

			udb();

			// set mmu page table
			// mmu_switch_ttbr(get_mmu_ctx(UOS_KERNEL_THREAD_ID));
			// mmu_set_ttbr0(get_mmu_ctx(UOS_KERNEL_THREAD_ID));

			udb();

			// return back to kernel
			current_thread = UOS_KERNEL_THREAD_ID;

			uart_print("Now kernel jumping to: ");
			uart_print_hex64(threads[UOS_KERNEL_THREAD_ID].context.pc);
			uart_print("\n");

			udb();
			context_load(&threads[UOS_KERNEL_THREAD_ID].context);
		}
		else
		{
			// load next thread
			job_done++;
			current_thread = next;

			// check is it el1 thread
			if (threads[current_thread].flags & UTH_FLAG_EL1_BIT)
			{
				// set mmu page table
				//mmu_set_ttbr0(get_mmu_ctx(current_thread));
				//mmu_set_ttbr1(get_mmu_ctx(current_thread));
			}
			else
			{
				// set mmu page table (dont touch el1! otherwise it will blow up[not sure])
				// there is must be different & separate table for only el0 threads...
				//mmu_set_ttbr0(get_mmu_ctx(current_thread));
			}

			// reset irq
			u_arm_write_cntp_tval_el0(u_arm_read_cntfrq() / (1000 / UTH_BURST_TIME));

			context_load(&threads[current_thread].context);
		}
	}
	else
	{
		// disable irq
		u_arm_disable_cntp_irq();

		// set mmu page table
		//mmu_set_ttbr0(get_mmu_ctx(UOS_KERNEL_THREAD_ID));
		//mmu_set_ttbr1(get_mmu_ctx(UOS_KERNEL_THREAD_ID));

		// return back to kernel
		current_thread = UOS_KERNEL_THREAD_ID;

		context_load(&threads[UOS_KERNEL_THREAD_ID].context);
	}
}

mmu_context_t *get_mmu_ctx(uint64_t thread_id)
{
	return threads[thread_id].mmu_ctx;
}

void u_yield()
{
	//if(current_thread != UOS_DUMMY_THREAD_ID);
	/*
		uart_print("yield by: ");
		uart_print_dec(current_thread);
		uart_print("\n");
		*/
	if (current_thread == UOS_KERNEL_THREAD_ID)
	{
		// if we are on kernel (id == 0)
		uart_print("Kernel yield.\n");
		// normal run
		u_thread_run_threads_(128);
	}
	else
	{
		// if we are on other thread

		// instant irq for switching next thread
		u_arm_enable_cntp_irq();
		u_arm_instant_cntp_irq();
	}
}
