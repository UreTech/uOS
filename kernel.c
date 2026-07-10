/*
uOS Kernel Version: U25Q4-AArch64-Urik4
uX Version: Kernel Embed (look u_uX.h for full version name)

Expected version calendar:
- Alpha versions
U25Q4-AArch64-Urik4 <-
U26Q1-AArch64-Urik5
U26Q1-AArch64-Urik6
U26Q1-AArch64-Urik7
...

- E1 Release
U26Q2-AArch64-UrikAsit-E1

- E2 beta
U26Q2-AArch64-UrikAsit-E2-d00
...
U26Q3-AArch64-UrikAsit-E2-d10

- E2 Release
U26Q3-AArch64-UrikAsit-E2

-E3s beta (Security Enhanced)
U26Q4-AArch64-UrikAsit-E3s-d00
...
U27Q1-AArch64-UrikAsit-E3s-d10

- E3s Release (Security Enhanced)
U27Q1-AArch64-UrikAsit-E3s
*/

/*
uOS info code structure
<A:B:C:D>

A: Info type
-UIN: Info
-UWR: Warn
-UER: Error
-UFE: Fatal Error

B: Location id
-Ke: Kernel entry
-Au: Api unkown
-Ux: uX Api

C: Section of B
-S[X]: X. modified or added section

D: Section name
-SD: SD card
-UFS: uFileSystem
-UX: uX Api
*/

#include "u_gpio.h" // basic gpio
#include "u_uart.h"

#include "u_display.h" // basic display
#include "u_rand.h"	   // basic random

#include <memory/u_memory.h>

#include <u_SuperRH.h>

#include "u_spi_flash.h"

#include "u_cstr_util.h"

#include "u_terminal.h"

#include "u_fs.h"

#include "u_arm.h"

#include "u_thread.h"

#include "u_mmu.h"

#include "u_uX.h"

// Pixel565 *mainDisplayBuffer;
u64 elapsedMS;
time_point start, end;
size_t freeBytes;

char tmpstr[40];

void start_sd(void)
{
	// Initialize the SD card
	if (sd_init() != SD_OK)
	{
		uart_print("SD boot init fail!\n<UER:Ke:S0:SD>\n");
		return;
	}

	// Get capacity in bytes
	uint64_t size_bytes = sd_get_size();

	// Get capacity in blocks (1 block = 512 bytes)
	uint32_t block_count = sd_get_block_count();

	// Print results
	uart_print("SDsize:\n");
	uart_print("  Bytes: ");
	uart_print(ulltoa(size_bytes, tmpstr));
	uart_print("\n");

	char *tstr[3];

	tstr[0] = "SDsize: ";
	tstr[1] = ftoa((float)size_bytes / (1024.0f * 1024.0f * 1024.0f), 2, tmpstr);
	tstr[2] = " Gb";

	char *astr = append_strs((char **)&tstr, 3);
	kfree(astr);
	/*
	if (ufs_init_sd() == UFS_FAIL)
	{
	uart_print("Formating sd...\n");
	if (ufs_format_sd() == UFS_SUCCESS)
	{
	uart_print("Format OK!\n");
	ut_print("Format OK!");
	}
	else
	{
	uart_print("Format ERR!\n");
	ut_print("Format ERR!");
	}
	}
	*/
}

__attribute__((optnone)) uint64_t this_must_return_input(uint64_t input)
{
	return input;
}

void t1()
{
	uart_print("T1 Joined!\n");
	while (1)
	{
		delay_ms(1002);
		uart_print("Im 1.002s!\n");
	}
}

void t2()
{
	uart_print("T2 Joined!\n");
	while (1)
	{
		delay_ms(1001);
		uart_print("Im 1.001s!\n");
	}
}

// unused here
extern void el1_sync_handler();
extern void irq_vector_entry();

void main()
{
	uart_init(UA_B115200);
	led_init();

	vbar_set(_el1_vectors_);

	uart_print("kernel wake up!\n");

	uart_print("uart_print addr: 0x");
	uart_print_hex64((uint64_t)&uart_print);
	uart_print(" mmu_enable addr: 0x");
	uart_print_hex64((uint64_t)&mmu_enable);
	uart_print("\n");

	// some systems use this variable so pre assigning prevents too many bugs
	current_thread = UOS_KERNEL_THREAD_ID;

	// init page allocator
	uart_print("Starting UMP system...\n");
	ump_allocator_init_mem();
	ump_allocator_init_vmem();
	uart_print("UMP system started!\n");

	// init heap allocator
	uart_print("Starting UHP system...\n");
	init_kernel_base_heaps();
	uart_print("UHP system started!\n");

	// init Super Request Handler (SRH)
	SRH_init();
	uart_print("Super Request Handler intiated!\n");

	// init thread system
	u_thread_initsys_();
	uart_print("thread system intiated!\n");

	// mmu is off here

	// calculate kernel section sizes
	size_t _kernel_text_size_ = _kernel_text_end_ - _kernel_text_start_;
	size_t _kernel_rodata_size_ = _kernel_rodata_end_ - _kernel_rodata_start_;
	size_t _kernel_data_size_ = _kernel_data_end_ - _kernel_data_start_;
	size_t _kernel_bss_size_ = _kernel_bss_end_ - _kernel_bss_start_;

	// map kernel text
	mmu_map_range(_kernel_text_start_, _kernel_text_start_, _kernel_text_size_, PTE_PRESET_KERNEL_EXECUTABLE, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// map kernel bss
	mmu_map_range(_kernel_bss_start_, _kernel_bss_start_, _kernel_bss_size_, PTE_PRESET_KERNEL_DATA, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// map kernel data
	mmu_map_range(_kernel_data_start_, _kernel_data_start_, _kernel_data_size_, PTE_PRESET_KERNEL_DATA, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// map kernel rodata
	mmu_map_range(_kernel_rodata_start_, _kernel_rodata_start_, _kernel_rodata_size_, PTE_PRESET_KERNEL_RODATA, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// map kernel stack
	mmu_map_range(_kernel_stack_start_pa_ptr_, _kernel_stack_start_pa_ptr_, _kernel_stack_end_pa_ptr_ - _kernel_stack_start_pa_ptr_, PTE_PRESET_KERNEL_DATA, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// map all of pageable memory
	mmu_map_range(mem, mem, mem_end - mem, PTE_PRESET_KERNEL_DATA, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// map all of pageable video memory
	mmu_map_range(vmem, vmem, vmem_end - vmem, PTE_PRESET_KERNEL_DATA, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// Main peripherals (GPIO, UART, SPI, I2C, etc.)
	// Base: 0xFE000000, Size: ~16MB
	mmu_map_range(0xFE000000, 0xFE000000, 0x1000000, PTE_PRESET_KERNEL_MMIO, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// PCIe controller registers
	// Base: 0xFD500000
	mmu_map_range(0xFD500000, 0xFD500000, 0xF0000, PTE_PRESET_KERNEL_MMIO, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// GIC-400 (Interrupt Controller)
	// Base: 0xFF840000
	mmu_map_range(0xFF840000, 0xFF840000, 0xF0000, PTE_PRESET_KERNEL_MMIO, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// Local peripherals (ARM timer, local interrupts)
	// Base: 0xFF800000
	mmu_map_range(0xFF800000, 0xFF800000, 0x40000, PTE_PRESET_KERNEL_MMIO, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// also map kernel stuff to high va

	// map kernel text
	mmu_map_range(UOS_TTBR1_MIN + _kernel_text_start_, _kernel_text_start_, _kernel_text_size_, PTE_PRESET_KERNEL_EXECUTABLE, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// map kernel bss
	mmu_map_range(UOS_TTBR1_MIN + _kernel_bss_start_, _kernel_bss_start_, _kernel_bss_size_, PTE_PRESET_KERNEL_DATA, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// map kernel data
	mmu_map_range(UOS_TTBR1_MIN + _kernel_data_start_, _kernel_data_start_, _kernel_data_size_, PTE_PRESET_KERNEL_DATA, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// map kernel rodata
	mmu_map_range(UOS_TTBR1_MIN + _kernel_rodata_start_, _kernel_rodata_start_, _kernel_rodata_size_, PTE_PRESET_KERNEL_RODATA, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// map kernel stack
	mmu_map_range(UOS_TTBR1_MIN + _kernel_stack_start_pa_ptr_, _kernel_stack_start_pa_ptr_, _kernel_stack_end_pa_ptr_ - _kernel_stack_start_pa_ptr_, PTE_PRESET_KERNEL_DATA, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// enable mmu

	uart_print("ttbr0: 0x");
	uart_print_hex64(get_mmu_ctx(UOS_KERNEL_THREAD_ID)->L0_ttbr0);
	uart_print(" ttbr1: 0x");
	uart_print_hex64(get_mmu_ctx(UOS_KERNEL_THREAD_ID)->L0_ttbr1);
	uart_print("\n");

	uart_print("Enabling mmu...\n");
	mmu_setup(); // setup mmu registers

	mmu_switch_ttbr(get_mmu_ctx(UOS_KERNEL_THREAD_ID));
	mmu_barrier();
	mmu_flush_tlb();

	mmu_enable();

	uart_print("Mmu enabled!\n");

	// try to jump to high va kernel
	uint64_t high_va_offset = UOS_TTBR1_MIN;

	asm volatile(
		"add sp, sp, %0\n"
		"add x29, x29, %0\n"
		"adr x0, 1f\n"
		"add x0, x0, %0\n"
		"br x0\n"
		"1:\n"
		:
		: "r"(high_va_offset)
		: "x0", "memory");

	// reset vbar
	vbar_set(UOS_TTBR1_MIN + _high_va_el1_vectors_);

	uart_print("We are in high va! \nPC: 0x");
	uart_print_hex64(read_pc());
	uart_print("\nSP: 0x");
	uart_print_hex64(read_sp());
	uart_print("\nLR: 0x");
	uart_print_hex64(read_lr());
	uart_print("\n");

	// now we are in high va kernel

	uart_print("Unmapping old low va\n");
	// unmap old pa=va kernel bss/code/stack region

	// unmap old kernel code & bss other data..

	//mmu_unmap_range(get_mmu_ctx(UOS_KERNEL_THREAD_ID), _kernel_start_pa_ptr_, _kernel_end_pa_ptr_ - _kernel_start_pa_ptr_);

	// unmap old kernel stack
	// nope mmu_unmap_range(get_mmu_ctx(UOS_KERNEL_THREAD_ID), _kernel_stack_start_pa_ptr_, _kernel_stack_end_pa_ptr_ - _kernel_stack_start_pa_ptr_ - (4096));

	uart_print("MMU enabled successfuly!\n");

	// reinit Super Request Handler (SRH) because handler addresses are changed
	SRH_init();

	uart_print("Super Request Handler reintiated!\n");

	uart_print("EL: ");
	uart_print_dec(u_arm_current_el_());
	uart_print("\n");

	delay_ms(1000);

	srand(0x330633);

	int *a = 0ULL;
	*a = 0;

	while (true)
		; // wait here

	/*
		lcd_init();
	lcd_clear_screen(0xffff);

	lcd_clear_screen(P565(255, 0, 255));
*/
	char *usrinp = (char *)kmalloc(300);
	*usrinp = '\0';

	char *terinp = (char *)kmalloc(300);
	*terinp = '\0';

	spi_init();

	// test stuff
	void *ptr = NULL;
	void *oPtr = ptr;

	start_sd();

	uart_print("kernel loop start\n");
	while (1)
	{
		if (uart_available())
		{
			uart_input(terinp, 300);

			if (!strcmp(terinp, "print"))
			{
				uart_print("OK!\n");
			}
			else if (!strcmp(terinp, "tTest"))
			{
				uart_print("Creating threads!\n");
				u_thread_create(t1, 10, 1);
				uart_print("OK! 1\n");
				u_thread_create(t2, 10, 1);
				uart_print("OK! 2\n");
				uart_print("Threads created!\n");
			}
			else if (!strcmp(terinp, "cause_mem_exception"))
			{
				uart_print("Causing access exception...\n");
				((char *)((uint64_t)(-1)))[0] = 'E';
			}
			else if (!strcmp(terinp, "ufs_format"))
			{
				uart_print("Formating sd...\n");
				if (ufs_format_sd() == UFS_SUCCESS)
				{
					uart_print("Format OK!\n");
				}
				else
				{
					uart_print("Format ERR!\n");
				}
				uart_print("SD UFS formated!\n");
			}
			else if (!strcmp(terinp, "ufs_init"))
			{
				if (ufs_init_sd() == UFS_FAIL)
				{
					uart_print("UFS init error!\n");
				}
				else
				{
					uart_print("UFS init success!\n");
				}
			}
			else if (!strcmp(terinp, "mstat"))
			{
				udb();
			}
			else if (!strcmp(terinp, "sdinit"))
			{
				uart_print("Disabled\n");
			}

			else
			{
				uart_print(terinp);
				uart_print(" -> Unknown interrupt command!\n");
			}
		}

		//run threads with 128 ctx switch max
		//u_thread_run_threads_(128);

		// process SuperRH requests
		SRH_process_requests();
	}

	while (1)
		;
}
