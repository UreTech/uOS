/*
uOS Kernel Version: U26Q3-AArch64-Urik4
*/

#include <u_kernel/drivers/gpio/u_gpio.h> // basic gpio
#include <u_kernel/drivers/uart/u_uart.h>

// #include "u_display.h" // basic display
#include <u_kernel/util/random/u_rand.h>   // hardware & seed random

#include <u_kernel/memory/u_memory.h>

#include <u_kernel/framework/SupervisorRequestHandler/u_SuperRH.h>

#include <u_kernel/drivers/emmc/u_emmc.h>

#include <u_kernel/filesystem/vfs/vfs.h>
#include <u_kernel/filesystem/ufs/ufs.h>
#include <u_kernel/filesystem/fat/fat.h>

#include <u_kernel/util/u_cstr_util.h>

#include <u_kernel/arch/arm/u_arm.h>

#include <u_kernel/framework/threading/u_thread.h>

#include <u_kernel/memory/u_mmu.h>

#include <u_kernel/uFX/u_uFX.h>

u64 elapsedMS;
time_point start, end;
size_t freeBytes;

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

static inline void disable_dcache(void)
{
    uint64_t sctlr;

    asm volatile(
        "mrs %0, sctlr_el1\n"
        "bic %0, %0, #(1 << 2)\n"
        "msr sctlr_el1, %0\n"
        "isb\n"
        : "=r"(sctlr)
        :
        : "memory"
    );

	uint64_t sctlr__;
asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr__));
uart_print("SCTLR_EL1 M bit (MMU enable): ");
uart_print_dec(sctlr__ & 0x1);
uart_print("\n");
}

// unused here
extern void el1_sync_handler();
extern void irq_vector_entry();

void main()
{
	uart_init(UA_B115200);
	led_init();
	srand(0x330633);
	init_hardware_rng();

	vbar_set(_el1_vectors_);

	uart_print("kernel wake up!\n");

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

	// init vfs
	vfs_init();
	// create common vfs directories
    vfs_create_directory("/", "devices");
    vfs_create_directory("/devices", "storage");
    vfs_create_directory("/devices", "gpio");
    vfs_create_directory("/devices", "uart");
    vfs_create_directory("/devices", "hardwareRNG");

    vfs_create_directory("/", "parts"); // partitions
 
    vfs_create_directory("/", "uOS");
    vfs_create_directory("/uOS", "info");
    vfs_create_directory("/uOS/info", "aarch64");

	// init uobject
	uobject_init_uobject_tables();

	emmc_init();

	uart_print("SD init...\n");
	if(emmc_init_sd_card() == SUCCESS){
		uart_print("SD init successful!\n");
		uobject_ref emmc_storage_device = vfs_get_device_ref("/devices/storage/emmc0");
		if(format_sd_gpt_with_pre_partitions(emmc_storage_device) == UFS_SUCCESS){
			uart_print("GPT partitions created!\n");
		}else{
			uart_print("GPT partition creation failed!\n");
		}
	}else{
		uart_print("SD init failed!\n");
	}

	while(true);

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
	
	// unmap low va kernel text
	mmu_unmap_range(_kernel_text_start_, _kernel_text_size_, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// unmap low va kernel bss
	mmu_unmap_range(_kernel_bss_start_, _kernel_bss_size_, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// unmap low va kernel data
	mmu_unmap_range(_kernel_data_start_, _kernel_data_size_, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// unmap low va kernel rodata
	mmu_unmap_range(_kernel_rodata_start_, _kernel_rodata_size_, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	// unmap low va kernel stack
	mmu_unmap_range(_kernel_stack_start_pa_ptr_, _kernel_stack_end_pa_ptr_ - _kernel_stack_start_pa_ptr_, get_mmu_ctx(UOS_KERNEL_THREAD_ID));

	uart_print("MMU enabled successfuly!\n");

	// reinit Super Request Handler (SRH) because handler addresses are changed
	SRH_init();

	uart_print("Super Request Handler reintiated!\n");

	uart_print("EL: ");
	uart_print_dec(u_arm_current_el_());
	uart_print("\n");

	delay_ms(1000);

	uart_print("inf wait\n");

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

	// test stuff
	void *ptr = NULL;
	void *oPtr = ptr;

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
				uart_print("SD UFS formated!\n");
			}
			else if (!strcmp(terinp, "ufs_init"))
			{
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
