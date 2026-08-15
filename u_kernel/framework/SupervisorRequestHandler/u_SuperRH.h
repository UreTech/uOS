#ifndef U_SUPERRH_H
#define U_SUPERRH_H

#include <u_kernel/util/u_ctypes.h>
#include <u_kernel/framework/threading/u_thread.h>

// SRH Call ids
#define SRH_CALL_NULL 0ULL
#define SRH_CALL_KMALLOC 1ULL
#define SRH_CALL_KVMALLOC 2ULL
#define SRH_CALL_KRELOC 3ULL
#define SRH_CALL_KVRELOC 4ULL
#define SRH_CALL_KFREE 5ULL
#define SRH_CALL_KVFREE 6ULL
#define SRH_CALL_PMALLOC 7ULL
#define SRH_CALL_VPMALLOC 8ULL
#define SRH_CALL_PFREE 9ULL
#define SRH_CALL_VPFREE 10ULL

// handle count
#define SRH_HCOUNT 11

// Supervisor Request Handler

// return result
// x0 or argument 1: input ptr
typedef void (*SVC_FUNCPTR)(void *);

void SRH_init();

void SRH_request_call(void *args, uint64_t handleID);

void SRH_process_requests();

// flush request when thread is dead
void SRH_flush_request(uint64_t threadID);

#endif