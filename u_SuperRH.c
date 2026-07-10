#include "u_SuperRH.h"
#include "u_mutex.h"

typedef enum
{
	SRH_REQ_ERR = 0,
	SRH_REQ_IN_QUEUE = 1,
	SRH_REQ_RESPONDED = 2,
	SRH_REQ_IDLE = 3,
	SRH_REQ_EMPTY = 4,
} SRH_Req_status;

typedef struct
{
	void *arg;
	uint64_t handleID;
	SRH_Req_status status;
} SRH_Request;

SRH_Request tREQtable[UTH_MAX_THREADS];
uint64_t reqQueue[UTH_MAX_THREADS];
size_t reqCount = 0;

u_mutex reqMtx;

SVC_FUNCPTR SRH_HANDLERS[SRH_HCOUNT];

void _SRH_HANDLE_NULL_(void *arg)
{
	return;
}

// forward declarations for handles
// uhp
void _SRH_HANDLE_kmalloc_(void *args);
void _SRH_HANDLE_kvmalloc_(void *args);
void _SRH_HANDLE_kreloc_(void *args);
void _SRH_HANDLE_kvreloc_(void *args);
void _SRH_HANDLE_kfree_(void *args);
void _SRH_HANDLE_kvfree_(void *args);
// ump
void _SRH_HANDLE_pmalloc_(void *args);
void _SRH_HANDLE_vpmalloc_(void *args);
void _SRH_HANDLE_pfree_(void *args);
void _SRH_HANDLE_vpfree_(void *args);

void SRH_init()
{
	// init mutex etc.
	for (size_t i = 0; i < UTH_MAX_THREADS; i++)
	{
		tREQtable[i].status = SRH_REQ_EMPTY;
	}

	extern uint64_t global_var;
	uart_print("GLOBAL: ");
	uart_print_hex64((uint64_t)&reqCount);
	uart_print("\n");

	uint64_t *ptr = &reqCount;
	//	udb();
	*ptr = 123;
	//	reqCount = 0;
	//udb();

	// init mutex
	u_mutex_init(&reqMtx);

	// set handles
	SRH_HANDLERS[SRH_CALL_NULL] = _SRH_HANDLE_NULL_;
	SRH_HANDLERS[SRH_CALL_KMALLOC] = _SRH_HANDLE_kmalloc_;
	SRH_HANDLERS[SRH_CALL_KVMALLOC] = _SRH_HANDLE_kvmalloc_;
	SRH_HANDLERS[SRH_CALL_KRELOC] = _SRH_HANDLE_kreloc_;
	SRH_HANDLERS[SRH_CALL_KVRELOC] = _SRH_HANDLE_kvreloc_;
	SRH_HANDLERS[SRH_CALL_KFREE] = _SRH_HANDLE_kfree_;
	SRH_HANDLERS[SRH_CALL_KVFREE] = _SRH_HANDLE_kvfree_;

	SRH_HANDLERS[SRH_CALL_PMALLOC] = _SRH_HANDLE_pmalloc_;
	SRH_HANDLERS[SRH_CALL_VPMALLOC] = _SRH_HANDLE_vpmalloc_;
	SRH_HANDLERS[SRH_CALL_PFREE] = _SRH_HANDLE_pfree_;
	SRH_HANDLERS[SRH_CALL_VPFREE] = _SRH_HANDLE_vpfree_;
}

void SRH_request_call(void *args, uint64_t handleID)
{
	if (current_thread == UOS_KERNEL_THREAD_ID)
	{
		// do not add to queue if caller is kernel
		if (handleID > SRH_HCOUNT)
		{
			uart_print("Kernel tried to use invalid SuperHandle ID\n");
		}
		else
		{
			// run handler
			SRH_HANDLERS[handleID](args);
		}
	}
	else
	{
		if (handleID > SRH_HCOUNT)
		{
			uart_print("A thread tried to use invalid SuperHandle ID\n");
		}
		else
		{
			// Add to request queue
			u_mutex_lock_el1(&reqMtx); // lock

			reqQueue[reqCount] = current_thread;

			tREQtable[current_thread].arg = args;
			tREQtable[current_thread].handleID = handleID;
			tREQtable[current_thread].status = SRH_REQ_IN_QUEUE;

			reqCount++;

			u_mutex_unlock_el1(&reqMtx); // unlock

			// Wait for response
			while (1)
			{
				switch (tREQtable[current_thread].status)
				{
				case (SRH_REQ_RESPONDED):
					// response write to arg so just return
					return;
				case (SRH_REQ_IN_QUEUE):
					u_yield(); // not block other threads
					continue;
				case (SRH_REQ_ERR):
					uart_print("Response error!(ERR)\n");
					return;
				case (SRH_REQ_IDLE):
					uart_print("Unexpected response error!(IDLE)\n");
					return;
				default:
					uart_print("Unknown response error!\n");
					return;
				};
			}
		}
	}
}

void SRH_process_requests()
{
	// not needed but idk :p
	u_mutex_lock_el1(&reqMtx); // lock

	// process all requests
	for (int i = 0; i < reqCount; i++)
	{
		// run handler
		SRH_HANDLERS[tREQtable[reqQueue[i]].handleID](tREQtable[reqQueue[i]].arg);

		// set req status
		tREQtable[reqQueue[i]].status = SRH_REQ_RESPONDED;
	}

	// reset counter
	reqCount = 0;

	u_mutex_unlock_el1(&reqMtx); // unlock
}

// flush
void SRH_flush_request(uint64_t threadID)
{
	u_mutex_lock_el1(&reqMtx);					// lock
	tREQtable[threadID].status = SRH_REQ_EMPTY; // set to empty
	u_mutex_unlock_el1(&reqMtx);				// unlock
}
