#ifndef U_ARM_H
#define U_ARM_H

#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/drivers/uart/u_uart.h>

#define PI4_AXI_BUS_ADDRESS_SPACE_OFFSET (0xC0000000ULL)

//#define ARM_EXC_PRNT_SCR_ENABLE

unsigned int u_arm_current_el_(void);

void print_exception_info(uint64_t esr, uint64_t far, uint64_t elr, uint64_t spsr);

/*
uint32_t u_arm_get_cpu_temp_(void);

uint32_t u_arm_get_cpu_freq_(void);

int u_arm_set_cpu_freq_(uint32_t freq_mhz);
*/

#endif