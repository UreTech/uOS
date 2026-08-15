#ifndef U_THREAD_H
#define U_THREAD_H

#include <u_kernel/memory/u_memory.h>

#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/drivers/uart/u_uart.h>
#include <u_kernel/timer/u_timer.h>
#include <u_kernel/memory/u_mmu.h>
#define UTH_SUCCESS 0
#define UTH_FAIL 1

#define UOS_KERNEL_THREAD_ID 0
#define UOS_KERNEL_ID 0
#define UTH_MAX_THREADS 2048

// 7ms is enough for now
#define UTH_BURST_TIME 7

// never called (null context for destroyed waiting threads)
#define UOS_DUMMY_THREAD_ID 1

#define UTH_FLAG_READY_BIT ONEBIT(0)
#define UTH_FLAG_NULL_BIT ONEBIT(1)
#define UTH_FLAG_EL1_BIT ONEBIT(2)

extern uint64_t current_thread;

void u_thread_initsys_();

void u_thread_run_threads_(uint32_t ttr);

uint32_t u_thread_create(void (*entry)(void), uint64_t priority, int is_el1);

uint32_t u_thread_destroy(uint64_t thread_id);

// only thread 0 (kernel main thread) can use it properly
//typedef struct mmu_context_t;
mmu_context_t *get_mmu_ctx(uint64_t thread_id);

void u_yield();

#endif