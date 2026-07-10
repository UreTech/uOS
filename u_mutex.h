#ifndef U_MUTEX_H
#define U_MUTEX_H

#include "u_uart.h"
#include "u_ctypes.h"

typedef struct
{
	volatile uint64_t owner;
} u_mutex;

#define U_MUTEX_UNLOCKED ((uint64_t)0xDEADBEEF)

// kernel thread id = 0
// dummy thread id = 1

// --- TPIDR ---
static inline uint64_t get_owner_id_el1(void)
{
	uint64_t tp;
	__asm__ volatile("mrs %0, tpidr_el1"
					 : "=r"(tp));
	return tp; // 0 -> kernel
}

static inline uint64_t get_owner_id_el0(void)
{
	uint64_t tp;
	__asm__ volatile("mrs %0, tpidr_el0"
					 : "=r"(tp));
	return tp;
}

// --- init ---
static inline void u_mutex_init(u_mutex *m)
{
	m->owner = U_MUTEX_UNLOCKED;
}

// --- low-level CAS (atomic compare and swap) ---
// Returns 1 if successful, 0 if failed.
static inline uint32_t u_mutex_cas(volatile uint64_t *addr, uint64_t expected, uint64_t desired)
{
	uint64_t old;
	uint32_t result;

	__asm__ volatile(
		"   mov     %w1, #0         \n" // Default to failure
		"1: ldaxr   %0, [%2]        \n" // Load with acquire
		"   cmp     %0, %3          \n" // Compare with expected
		"   b.ne    2f              \n" // Branch if not equal
		"   stlxr   %w1, %4, [%2]   \n" // Try to store, result in w1
		"   cbnz    %w1, 1b         \n" // Retry if store failed
		"   mov     %w1, #1         \n" // Store succeeded, return 1
		"   b       3f              \n"
		"2: clrex                   \n" // Clear exclusive
		"3:                         \n"
		: "=&r"(old), "=&r"(result)				 // 2 outputs
		: "r"(addr), "r"(expected), "r"(desired) // 3 inputs
		: "cc", "memory");

	return result;
}

// --- try_lock ---
static inline uint32_t u_mutex_try_lock(u_mutex *m, uint64_t me)
{
	return u_mutex_cas(&m->owner, U_MUTEX_UNLOCKED, me);
}

// forward dec
void u_yield();

// --- lock (spin + WFE) ---
static inline void u_mutex_lock(u_mutex *m, uint64_t me)
{
	// Detect recursive locking BEFORE attempting lock
	if (m->owner == me)
	{
		uart_print("WARN: Recursive lock!\n");
		uart_print("me: ");
		uart_print_dec(me);
		uart_print("\n");
		return;
	}

	while (!u_mutex_try_lock(m, me))
	{
		/*
		uart_print("Still waiting lock...\n");
		uart_print("me: ");
		uart_print_dec(me);
		uart_print("\n");
		uart_print("owner: ");
		uart_print_dec(m->owner);
		uart_print("\n");
*/

		u_yield();
		//__asm__ volatile("wfe" ::
		//	 : "memory");
	}

	__asm__ volatile("" ::
						 : "memory"); // Compiler barrier
}

// --- unlock ---
static inline void u_mutex_unlock(u_mutex *m, uint64_t me)
{
	__asm__ volatile("" ::
						 : "memory"); // Compiler barrier

	if (!u_mutex_cas(&m->owner, me, U_MUTEX_UNLOCKED))
	{
		uart_print("ERROR: Unlock by non-owner!\n");
		uart_print_hex64(me);
		uart_print(" vs ");
		uart_print_hex64(m->owner);
		uart_print("\n");
		return;
	}

	//	__asm__ volatile("sev" ::
	//	 : "memory"); // Wake waiting threads
}

// --- EL1 ---

static inline uint32_t u_mutex_try_lock_el1(u_mutex *m)
{
	uint64_t me = get_owner_id_el1();
	return u_mutex_try_lock(m, me);
}

static inline void u_mutex_lock_el1(u_mutex *m)
{
	uint64_t me = get_owner_id_el1();
	u_mutex_lock(m, me);
}

static inline void u_mutex_unlock_el1(u_mutex *m)
{
	uint64_t me = get_owner_id_el1();
	u_mutex_unlock(m, me);
}

// --- EL0 ---

static inline uint32_t u_mutex_try_lock_el0(u_mutex *m)
{
	uint64_t me = get_owner_id_el0();
	return u_mutex_try_lock(m, me);
}

static inline void u_mutex_lock_el0(u_mutex *m)
{
	uint64_t me = get_owner_id_el0();
	u_mutex_lock(m, me);
}

static inline void u_mutex_unlock_el0(u_mutex *m)
{
	uint64_t me = get_owner_id_el0();
	u_mutex_unlock(m, me);
}

#endif