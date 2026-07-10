#ifndef U_ARM_H
#define U_ARM_H

#include "u_ctypes.h"
#include "u_uart.h"

//#define ARM_EXC_PRNT_SCR_ENABLE

unsigned int u_arm_current_el_(void);

void print_exception_info(uint64_t esr, uint64_t far, uint64_t elr, uint64_t spsr);

/*
uint32_t u_arm_get_cpu_temp_(void);

uint32_t u_arm_get_cpu_freq_(void);

int u_arm_set_cpu_freq_(uint32_t freq_mhz);
*/

#endif