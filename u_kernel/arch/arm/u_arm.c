#include <u_kernel/arch/arm/u_arm.h>

// active context owner (from u_thread.c)
extern uint64_t current_thread;

//debug
#include<u_kernel/drivers/uart/u_uart.h>

unsigned int u_arm_current_el_(void)
{
	unsigned long el;
	asm volatile("mrs %0, CurrentEL"
				 : "=r"(el));
	return (unsigned int)((el >> 2) & 0x3);
}

void esr_exception_flag_names(uint64_t esr)
{
#ifdef ARM_EXC_PRNT_SCR_ENABLE
	u_canvas *fscr = uc_new_canvas(128, 128, UC_RGBA5658, UC_SINGLE_BUFFER); // fart screen

	uc_clear_canvas(fscr, P5658(0, 130, 0, 255));

	// 9, 5, 9 was minimum 565 gray
	uc_draw_string("Fatal Fart!", 0, 0, P565(9, 5, 9), 255, fscr, false);
#endif

	uint32_t ec = (esr >> 26) & 0x3F;

	uint32_t il = (esr >> 25) & 0x1;

	uint32_t iss = esr & 0x1FFFFFF;

	uart_print("ESR=0x");
	uart_print_hex64(esr);
	uart_print(" | EC=0x");
	uart_print_hex8(ec);
	uart_print(" ");

	switch (ec)
	{
	case 0x00:
		uart_print("(Unknown reason)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Unknown reason", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x01:
		uart_print("(Trapped WFI/WFE)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Trapped WFI/WFE", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x03:
		uart_print("(Trapped MCR/MRC to CP15)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Trapped MCR/MRC to CP15", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x04:
		uart_print("(Trapped MCRR/MRRC to CP15)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Trapped MCRR/MRRC to CP15", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x05:
		uart_print("(Trapped MCR/MRC to CP14)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Trapped MCR/MRC to CP14", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x06:
		uart_print("(Trapped LDC/STC to CP14)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Trapped LDC/STC to CP14", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x07:
		uart_print("(Access to SVE/SIMD/FP registers)");
		uart_print(" | TFV=");
		uart_print_dec((iss >> 23) & 1);
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Access to SVE/SIMD/FP registers", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x0C:
		uart_print("(Trapped MRRC to CP14)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Trapped MRRC to CP14", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x0E:
		uart_print("(Illegal Execution State)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Illegal Execution State", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x11:
		uart_print("(SVC instruction execution in AArch32)");
		uart_print(" | imm16=0x");
		uart_print_hex16(iss & 0xFFFF);
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("SVC instruction execution in AArch32", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x15:
		uart_print("(SVC instruction execution in AArch64)");
		uart_print(" | imm16=0x");
		uart_print_hex16(iss & 0xFFFF);
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("SVC instruction execution in AArch64", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x18:
		uart_print("(Trapped MSR/MRS/System instruction)");
		uart_print(" | dir=");
		uart_print((iss & (1 << 0)) ? "read" : "write");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Trapped MSR/MRS/System instruction", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x19:
		uart_print("(Access to SVE functionality)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Access to SVE functionality", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x20:
		uart_print("(Instruction Abort from lower EL)");
		uart_print(" | FSC=0x");
		uart_print_hex8(iss & 0x3F);
		uart_print(" | S1PTW=");
		uart_print_dec((iss >> 7) & 1);
		uart_print(" | FnV=");
		uart_print_dec((iss >> 10) & 1);
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Instruction Abort from lower EL", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x21:
		uart_print("(Instruction Abort from same EL)");
		uart_print(" | FSC=0x");
		uart_print_hex8(iss & 0x3F);
		uart_print(" | S1PTW=");
		uart_print_dec((iss >> 7) & 1);
		uart_print(" | FnV=");
		uart_print_dec((iss >> 10) & 1);
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Instruction Abort from same EL", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x22:
		uart_print("(PC alignment fault)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("PC alignment fault", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x24:
	case 0x25:
	{
		uart_print((ec == 0x24) ? "(Data Abort from lower EL)" : "(Data Abort from same EL)");
		uart_print(" | ISV=");
		uart_print_dec((iss >> 24) & 1);
		uart_print(" | SAS=0x");
		uart_print_hex8((iss >> 22) & 3);
		uart_print(" | SSE=");
		uart_print_dec((iss >> 21) & 1);
		uart_print(" | SRT=");
		uart_print_dec((iss >> 16) & 0x1F);
		uart_print(" | SF=");
		uart_print_dec((iss >> 15) & 1);
		uart_print(" | AR=");
		uart_print_dec((iss >> 14) & 1);
		uart_print(" | FnV=");
		uart_print_dec((iss >> 10) & 1);
		uart_print(" | WnR=");
		uart_print((iss & (1 << 6)) ? "Write" : "Read");
		uart_print(" | FSC=0x");
		uart_print_hex8(iss & 0x3F);

		uint32_t fsc = iss & 0x3F;
		uart_print(" (");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("DA@Same/Lower", 0, 9, P565(255, 0, 0), 255, fscr, false);
#endif

		switch (fsc & 0x3C)
		{
		case 0x00:
			uart_print("Address size fault, level ");
			uart_print_dec(fsc & 3);
#ifdef ARM_EXC_PRNT_SCR_ENABLE
			uc_draw_string("Address size fault", 0, 18, P565(255, 0, 255), 255, fscr, true);
#endif
			break;
		case 0x04:
			uart_print("Translation fault, level ");
			uart_print_dec(fsc & 3);
#ifdef ARM_EXC_PRNT_SCR_ENABLE
			uc_draw_string("Translation fault", 0, 18, P565(255, 0, 255), 255, fscr, true);
#endif
			break;
		case 0x08:
			uart_print("Access flag fault, level ");
			uart_print_dec(fsc & 3);
#ifdef ARM_EXC_PRNT_SCR_ENABLE
			uc_draw_string("Access flag fault", 0, 18, P565(255, 0, 255), 255, fscr, true);
#endif
			break;
		case 0x0C:
			uart_print("Permission fault, level ");
			uart_print_dec(fsc & 3);
#ifdef ARM_EXC_PRNT_SCR_ENABLE
			uc_draw_string("Permission fault", 0, 18, P565(255, 0, 255), 255, fscr, true);
#endif
			break;
		case 0x10:
			uart_print("Synchronous external abort");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
			uc_draw_string("Synchronous external abort", 0, 18, P565(255, 0, 255), 255, fscr, true);
#endif
			break;
		case 0x18:
			uart_print("Synchronous parity/ECC error");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
			uc_draw_string("Synchronous parity/ECC error", 0, 18, P565(255, 0, 255), 255, fscr, true);
#endif
			break;
		case 0x20:
			uart_print("Alignment fault");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
			uc_draw_string("Alignment fault", 0, 18, P565(255, 0, 255), 255, fscr, true);
#endif
			break;
		case 0x30:
			uart_print("TLB conflict abort");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
			uc_draw_string("TLB conflict abort", 0, 18, P565(255, 0, 255), 255, fscr, true);
#endif
			break;
		default:
			uart_print("Unknown fault");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
			uc_draw_string("Unknown fault", 0, 18, P565(255, 0, 255), 255, fscr, true);
#endif
			break;
		}
		uart_print(")");
		break;
	}
	case 0x26:
		uart_print("(SP alignment fault)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("SP alignment fault", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x28:
		uart_print("(Floating-point exception from AArch32)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Floating-point exception from AArch32", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x2C:
		uart_print("(Floating-point exception from AArch64)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Floating-point exception from AArch64", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x2F:
		uart_print("(SError interrupt)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("SError interrupt", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x30:
		uart_print("(Breakpoint from lower EL)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Breakpoint from lower EL", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x31:
		uart_print("(Breakpoint from same EL)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Breakpoint from same EL", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x32:
		uart_print("(Software Step from lower EL)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Software Step from lower EL", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x33:
		uart_print("(Software Step from same EL)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Software Step from same EL", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x34:
		uart_print("(Watchpoint from lower EL)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Watchpoint from lower EL", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x35:
		uart_print("(Watchpoint from same EL)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Watchpoint from same EL", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x38:
		uart_print("(BKPT instruction in AArch32)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("BKPT instruction in AArch32", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x3A:
	case 0x3B:
		uart_print("(Vector Catch from AArch32)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Vector Catch from AArch32", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x3C:
		uart_print("(BRK instruction in AArch64)");
		uart_print(" | imm16=0x");
		uart_print_hex16(iss & 0xFFFF);
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("BRK instruction in AArch64", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	case 0x3D:
		uart_print("(BRK instruction in AArch64)");
		uart_print(" | imm16=0x");
		uart_print_hex16(iss & 0xFFFF);
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("BRK instruction in AArch64", 0, 9, P565(255, 0, 0), 255, fscr, true);
#endif
		break;
	default:
		uart_print("(Reserved/Unknown EC)");
#ifdef ARM_EXC_PRNT_SCR_ENABLE
		uc_draw_string("Reserved/Unknown EC", 0, 9, P565(255, 0, 127), 255, fscr, true);
#endif
		break;
	}

#ifdef ARM_EXC_PRNT_SCR_ENABLE
	lcd_draw_bitmap_(0, 0, fscr->buf, fscr->w, fscr->h);
	uc_destroy_canvas(fscr);
#endif

	uart_print(" | IL=");
	uart_print_dec(il);
	uart_print(" | ISS= 0x");
	uart_print_hex32(iss);
}

void far_exception_flag_names(uint64_t far)
{
	uart_print("FAR= 0x");
	uart_print_hex64(far);
	uart_print(" (Faulting Virtual Address)");
}

void print_exception_info(uint64_t esr, uint64_t far, uint64_t elr, uint64_t spsr)
{
	uart_print("\n=== KERNEL FART! (EXCEPTION) ===\n");

	esr_exception_flag_names(esr);
	uart_print("\n");

	far_exception_flag_names(far);
	uart_print("\n");

	uart_print("ELR= 0x");
	uart_print_hex64(elr);
	uart_print(" (Exception Link Register - return address)\n");

	uart_print("SPSR= 0x");
	uart_print_hex64(spsr);
	uart_print(" (Saved Program Status Register)\n");

	uint64_t sp = 0;
	asm volatile(
    	"mov %0, sp"
    	: "=r"(sp)
	);

	uart_print("SP= 0x");
	uart_print_hex64(sp);
	uart_print(" (Stack Pointer)\n");

	uint64_t sctlr;
	asm volatile("mrs %0, sctlr_el1"
				 : "=r"(sctlr));

	uart_print("SCTLR_EL1= 0x");
	uart_print_hex64(sctlr);
	uart_print("\n");

	// Decode SPSR
	uint32_t mode = spsr & 0xF;
	uart_print("  Mode: ");
	switch (mode)
	{
	case 0x0:
		uart_print("EL0t");
		break;
	case 0x4:
		uart_print("EL1t");
		break;
	case 0x5:
		uart_print("EL1h");
		break;
	case 0x8:
		uart_print("EL2t");
		break;
	case 0x9:
		uart_print("EL2h");
		break;
	default:
		uart_print("Unknown(0x");
		uart_print_hex8(mode);
		uart_print(")");
		break;
	}
	uart_print(" | D=");
	uart_print_dec((spsr >> 9) & 1); // Debug mask
	uart_print(" A=");
	uart_print_dec((spsr >> 8) & 1); // SError mask
	uart_print(" I=");
	uart_print_dec((spsr >> 7) & 1); // IRQ mask
	uart_print(" F=");
	uart_print_dec((spsr >> 6) & 1); // FIQ mask
	uart_print("\nCurrent Thread ID: ");
	uart_print_dec(current_thread);
	uart_print("\n=================\n\n");
}
