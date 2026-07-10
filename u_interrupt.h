#ifndef U_INTERRUPT_H
#define U_INTERRUPT_H

#include "u_ctypes.h"

void u_timer_interrupt_init_();

unsigned long long u_arm_read_cntfrq(void);

void u_arm_enable_cntp_irq(void);

void u_arm_disable_cntp_irq(void);

void u_arm_instant_cntp_irq(void);

void u_arm_write_cntp_tval_el0(uint64_t val);

extern char el1_vectors[];
static void *_el1_vectors_ = (void *)&el1_vectors;

extern char high_va_el1_vectors[];
static void *_high_va_el1_vectors_ = (void *)&high_va_el1_vectors;

void vbar_set(void *vtable);

#endif