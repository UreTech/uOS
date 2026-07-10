#include <memory/u_memory.h>
#include <u_SuperRH.h>

UHP_CONTEXT base_kernel_heap_context;
UHP_CONTEXT base_kernel_vheap_context;

void init_kernel_base_heaps()
{
	UHP_init_heap_context(&base_kernel_heap_context, MEMORY_REGION_MMR);
	UHP_init_heap_context(&base_kernel_vheap_context, MEMORY_REGION_VMMR);
}

// kmalloc
typedef struct
{
	void *o_allocated_block;
	size_t i_size;
} _kmalloc_SRH_args_;
void *kmalloc(size_t size)
{
	_kmalloc_SRH_args_ req;
	req.i_size = size;
	SRH_request_call(&req, SRH_CALL_KMALLOC);
	return req.o_allocated_block;
}
void _SRH_HANDLE_kmalloc_(void *args)
{
	_kmalloc_SRH_args_ *req = args;
	req->o_allocated_block = UHP_malloc(req->i_size, &base_kernel_heap_context);
	return;
}

// kreloc
typedef struct
{
	void *o_new_block;
	void *i_old_block;
	size_t i_size;
	size_t i_old_size;
} _kreloc_SRH_args_;
void *kreloc(void *oldBlock, size_t oldSize, size_t newSize)
{
	_kreloc_SRH_args_ req;
	req.i_size = newSize;
	req.i_old_size = oldSize;
	req.i_old_block = oldBlock;

	SRH_request_call(&req, SRH_CALL_KRELOC);
	return req.o_new_block;
}
void _SRH_HANDLE_kreloc_(void *args)
{
	_kreloc_SRH_args_ *req = args;

	req->o_new_block = UHP_malloc(req->i_size, &base_kernel_heap_context);
	if (req->o_new_block == nullptr)
	{
		req->o_new_block = req->i_old_block;
		return;
	}

	memcpy(req->i_old_block, req->o_new_block, req->i_old_size);

	kfree(req->i_old_block);
	return;
}

// kfree
typedef struct
{
	void *i_block;
} _kfree_SRH_args_;
void kfree(void *block)
{
	_kfree_SRH_args_ req;
	req.i_block = block;

	SRH_request_call(&req, SRH_CALL_KFREE);

	return;
}
void _SRH_HANDLE_kfree_(void *args)
{
	_kfree_SRH_args_ *req = args;

	UHP_free(req->i_block, &base_kernel_heap_context);
	return;
}

// kvmalloc
typedef struct
{
	void *o_allocated_block;
	size_t i_size;
} _kvmalloc_SRH_args_;
void *kvmalloc(size_t size)
{
	_kvmalloc_SRH_args_ req;
	req.i_size = size;
	SRH_request_call(&req, SRH_CALL_KVMALLOC);
	return req.o_allocated_block;
}
void _SRH_HANDLE_kvmalloc_(void *args)
{
	_kvmalloc_SRH_args_ *req = args;
	req->o_allocated_block = UHP_malloc(req->i_size, &base_kernel_vheap_context);
	return;
}

// kvreloc
typedef struct
{
	void *o_new_block;
	void *i_old_block;
	size_t i_size;
	size_t i_old_size;
} _kvreloc_SRH_args_;
void *kvreloc(void *oldBlock, size_t oldSize, size_t newSize)
{
	_kvreloc_SRH_args_ req;
	req.i_size = newSize;
	req.i_old_size = oldSize;
	req.i_old_block = oldBlock;

	SRH_request_call(&req, SRH_CALL_KVRELOC);
	return req.o_new_block;
}
void _SRH_HANDLE_kvreloc_(void *args)
{
	_kvreloc_SRH_args_ *req = args;

	req->o_new_block = UHP_malloc(req->i_size, &base_kernel_vheap_context);
	if (req->o_new_block == nullptr)
	{
		req->o_new_block = req->i_old_block;
		return;
	}

	memcpy(req->i_old_block, req->o_new_block, req->i_old_size);

	kvfree(req->i_old_block);
	return;
}

// kvfree
typedef struct
{
	void *i_block;
} _kvfree_SRH_args_;
void kvfree(void *block)
{
	_kvfree_SRH_args_ req;
	req.i_block = block;

	SRH_request_call(&req, SRH_CALL_KVFREE);

	return;
}
void _SRH_HANDLE_kvfree_(void *args)
{
	_kvfree_SRH_args_ *req = args;

	UHP_free(req->i_block, &base_kernel_vheap_context);
	return;
}

// palloc
typedef struct
{
	void *o_page;
	size_t i_page_count;
} _palloc_SRH_args_;
void *palloc(size_t page_count)
{
	_palloc_SRH_args_ req;
	req.i_page_count = page_count;

	SRH_request_call(&req, SRH_CALL_PMALLOC);

	return req.o_page;
}
void _SRH_HANDLE_pmalloc_(void *args)
{
	_palloc_SRH_args_ *req = args;

	req->o_page = ump_palloc(req->i_page_count);
	return;
}

// pfree
typedef struct
{
	void *i_first_page;
	size_t i_page_count;
} _pfree_SRH_args_;
void pfree(void *first_page, size_t page_count)
{
	_pfree_SRH_args_ req;
	req.i_first_page = first_page;
	req.i_page_count = page_count;

	SRH_request_call(&req, SRH_CALL_PFREE);
	return;
}
void _SRH_HANDLE_pfree_(void *args)
{
	_pfree_SRH_args_ *req = args;
	ump_pfree(req->i_first_page, req->i_page_count);
	return;
}

// vpalloc
typedef struct
{
	void *o_page;
	size_t i_page_count;
} _vpalloc_SRH_args_;
void *vpalloc(size_t page_count)
{
	_vpalloc_SRH_args_ req;
	req.i_page_count = page_count;

	SRH_request_call(&req, SRH_CALL_VPMALLOC);

	return req.o_page;
}
void _SRH_HANDLE_vpmalloc_(void *args)
{
	_vpalloc_SRH_args_ *req = args;

	req->o_page = ump_vpalloc(req->i_page_count);
	return;
}

// vpfree
typedef struct
{
	void *i_first_page;
	size_t i_page_count;
} _vpfree_SRH_args_;
void vpfree(void *first_page, size_t page_count)
{
	_vpfree_SRH_args_ req;
	req.i_first_page = first_page;
	req.i_page_count = page_count;

	SRH_request_call(&req, SRH_CALL_VPFREE);
	return;
}
void _SRH_HANDLE_vpfree_(void *args)
{
	_vpfree_SRH_args_ *req = args;
	ump_vpfree(req->i_first_page, req->i_page_count);
	return;
}

// ---
void memset(void *ptr, uint8_t value, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		((volatile uint8_t *)ptr)[i] = value;
	}
}

void memfill(volatile void *ptr, void *filler, size_t fSize, size_t times)
{
	for (size_t i = 0; i < times; i++)
	{
		memcpy((void *)ptr + (i * fSize), filler, fSize);
	}
}

void memcpy(void *dst, void *src, size_t len)
{
	for (size_t i = 0; i < len; i++)
	{
		((volatile uint8_t *)dst)[i] = ((volatile uint8_t *)src)[i];
	}
}

int memcmp(void *src1, void *src2, size_t len)
{
	int result = 0;
	for (size_t i = 0; i < len; i++)
	{
		if (((volatile uint8_t *)src1)[i] != ((volatile uint8_t *)src2)[i])
		{
			result++;
		}
	}
	return result;
}