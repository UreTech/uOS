#include <u_mmu.h>
#include <memory/u_memory.h>

void mmu_setup()
{
	uint64_t mair =
		(0x00ULL << 0) | // AttrIdx 0 = Device-nGnRnE
		(0xFFULL << 8);	 // AttrIdx 1 = Normal WB RA WA

	asm volatile(
		"msr mair_el1, %0"
		:
		: "r"(mair));

	uint64_t tcr =
		/* TTBR0 */
		(16ULL << 0) |	  // T0SZ = 48-bit VA
		(0b01ULL << 8) |  // IRGN0 = WBWA
		(0b01ULL << 10) | // ORGN0 = WBWA
		(0b11ULL << 12) | // SH0 = Inner Shareable
		(0b00ULL << 14) | // TG0 = 4KB

		/* TTBR1 */
		(16ULL << 16) |	  // T1SZ = 48-bit VA
		(0b01ULL << 24) | // IRGN1 = WBWA
		(0b01ULL << 26) | // ORGN1 = WBWA
		(0b11ULL << 28) | // SH1 = Inner Shareable
		(0b10ULL << 30) | // TG1 = 4KB

		/* Physical address size */
		(0b101ULL << 32); // IPS = 48-bit PA

	asm volatile(
		"msr tcr_el1, %0"
		:
		: "r"(tcr));

	asm volatile("isb");
}

// it does nothing for now but may be features could be added
mmu_context_t *mmu_create_context()
{
	mmu_context_t *mmu_ctx = kmalloc(sizeof(mmu_context_t));
	mmu_ctx->L0_ttbr0 = nullptr;
	mmu_ctx->L0_ttbr1 = nullptr;
	mmu_ctx->allocated = nullptr;
	return mmu_ctx;
}

#define MMU_L0_MSIZE (512 * 1024 * 1024 * 1024)
#define MMU_L1_MSIZE (1024 * 1024 * 1024)
#define MMU_L2_MSIZE (2 * 1024 * 1024)
#define MMU_L3_MSIZE (4 * 1024)

#define MMU_ENTRY_PER_PAGE 512ULL

void _mmu_ctx_add_node_(PageTableNode *node, mmu_context_t *ctx)
{
	if (ctx->allocated == nullptr)
	{
		ctx->allocated = node;
		return;
	}
	else
	{
		if (ctx->allocated->next == nullptr)
		{
			ctx->allocated->next = node;
			return;
		}
		else
		{
			node->next = ctx->allocated->next;
			ctx->allocated->next = node;
			return;
		}
	}
}

// removes & destroys the node
void _mmu_destroy_node_(uint64_t *table, mmu_context_t *ctx)
{
	PageTableNode *last = nullptr;
	PageTableNode *current = ctx->allocated;
	while (current != nullptr)
	{
		if (current->table == table)
		{
			// found!

			// remove from chain
			if (last == nullptr)
			{
				ctx->allocated = current->next;
			}
			else
			{
				last->next = current->next;
			}

			// free
			pfree(current->table, 1); // 1 page freed
			kfree(current);			  // meta data freed
			return;					  // done!
		}

		last = current;
		current = current->next;
	}
	udbP("MMU WARNING! Failed to find the allocated node for table! E0");
}

// little helper (returns 0 if table is empty & destroyed)
uint64_t _mmu_try_remove_table_(uint64_t *table, mmu_context_t *ctx)
{
	for (int i = 0; i < MMU_ENTRY_PER_PAGE; i++)
	{
		if ((table[i] & 0b11) != (0b00))
		{
			// valid page found! page remove failed
			return 1; // done
		}
	}

	// no any valid page found
	_mmu_destroy_node_(table, ctx); // destroyed
	return 0;						// done
}

void mmu_map_range(uint64_t va, uint64_t pa, uint64_t length, uint64_t flags, mmu_context_t *ctx)
{
	if ((va & 0xFFF) || (pa & 0xFFF))
	{
		udbP("MMU WARNING! Unaligned mapping! E0");
		return;
	}

	uint64_t remaining = ALIGN_UP(length, 4096);
	// uint64_t saved_va = va;
	// uint64_t saved_pa = pa;

	while (remaining)
	{
		uint64_t L0_idx = (va >> 39) & 0x1FF;
		uint64_t L1_idx = (va >> 30) & 0x1FF;
		uint64_t L2_idx = (va >> 21) & 0x1FF;
		uint64_t L3_idx = (va >> 12) & 0x1FF;

		uint8_t mapping_layer = 3;

		if (va % MMU_L2_MSIZE == 0 && pa % MMU_L2_MSIZE == 0 && MMU_L2_MSIZE <= remaining)
		{
			if (va % MMU_L1_MSIZE == 0 && pa % MMU_L1_MSIZE == 0 && MMU_L1_MSIZE <= remaining)
			{
				// 1gb aligned
				mapping_layer = 1; // to L1 layer
			}
			else
			{
				// 2mb aligned
				mapping_layer = 2; // to L2 layer
			}
		}
		else
		{
			// 4kb aligned
			mapping_layer = 3; // to L3 layer
		}

		uint64_t *L0 = nullptr;
		if (va >= UOS_TTBR1_MIN)
		{
			L0 = ctx->L0_ttbr1;
		}
		else
		{
			L0 = ctx->L0_ttbr0;
		}

		// create L0 if not exsist
		if (L0 == nullptr)
		{
			PageTableNode *node = kmalloc(sizeof(PageTableNode)); // meta data
			node->next = nullptr;
			node->table = palloc(1);		// actual table 4kb aligned
			memset(node->table, 0x0, 4096); // zero all entries

			// set to context
			L0 = node->table;
			if (va >= UOS_TTBR1_MIN)
			{
				ctx->L0_ttbr1 = L0;
			}
			else
			{
				ctx->L0_ttbr0 = L0;
			}

			// add to allocated list
			_mmu_ctx_add_node_(node, ctx);
		}

		// create L1 if not exsist
		if ((L0[L0_idx] & 0b11) == (0b00))
		{
			PageTableNode *node = kmalloc(sizeof(PageTableNode)); // meta data
			node->next = nullptr;
			node->table = palloc(1);		// actual table 4kb aligned
			memset(node->table, 0x0, 4096); // zero all entries

			// add to L0 table
			L0[L0_idx] = ((uint64_t)node->table | (0b11));

			// add to allocated list
			_mmu_ctx_add_node_(node, ctx);
		}
		else if ((L0[L0_idx] & 0b11) != (0b11))
		{
			// throw some error here!
			udbP("MMU WARNING! This virtual address is have conflicting table entry type! E1");
			return;
		}

		// get L1 table
		uint64_t *L1 = L0[L0_idx] & PTE_ADDR_MASK;

		// L1 entry
		if (mapping_layer == 1)
		{
			if ((L1[L1_idx] & 0b11) != (0b00))
			{
				// throw some error here!
				udbP("MMU WARNING! This virtual address is already mapped! E2");
				return;
			}
			// add L1 entry
			L1[L1_idx] = pa | flags | PTE_VALID;
			remaining -= MMU_L1_MSIZE;
			va += MMU_L1_MSIZE;
			pa += MMU_L1_MSIZE;
			continue; // done! next!
		}

		// create L2 if not exsist
		if ((L1[L1_idx] & 0b11) == (0b00))
		{
			PageTableNode *node = kmalloc(sizeof(PageTableNode)); // meta data
			node->next = nullptr;
			node->table = palloc(1);		// actual table 4kb aligned
			memset(node->table, 0x0, 4096); // zero all entries

			// add to L1 table
			L1[L1_idx] = ((uint64_t)node->table | (0b11));

			// add to allocated list
			_mmu_ctx_add_node_(node, ctx);
		}
		else if ((L1[L1_idx] & 0b11) != (0b11))
		{
			// throw some error here!
			udbP("MMU WARNING! This virtual address is have conflicting table entry type! E3");
			return;
		}

		// get L2 table
		uint64_t *L2 = L1[L1_idx] & PTE_ADDR_MASK;

		// L2 entry
		if (mapping_layer == 2)
		{
			if ((L2[L2_idx] & 0b11) != (0b00))
			{
				// throw some error here!
				udbP("MMU WARNING! This virtual address is already mapped! E4");
				return;
			}
			// add L2 entry
			L2[L2_idx] = pa | flags | PTE_VALID;
			remaining -= MMU_L2_MSIZE;
			va += MMU_L2_MSIZE;
			pa += MMU_L2_MSIZE;
			continue; // done! next!
		}

		// create L3 if not exsist
		if ((L2[L2_idx] & 0b11) == (0b00))
		{
			PageTableNode *node = kmalloc(sizeof(PageTableNode)); // meta data
			node->next = nullptr;
			node->table = palloc(1);		// actual table 4kb aligned
			memset(node->table, 0x0, 4096); // zero all entries

			// add to L2 table
			L2[L2_idx] = ((uint64_t)node->table | (0b11));

			// add to allocated list
			_mmu_ctx_add_node_(node, ctx);
		}
		else if ((L2[L2_idx] & 0b11) != (0b11))
		{
			// throw some error here!
			udbP("MMU WARNING! This virtual address is have conflicting table entry type! E5");
			return;
		}
		// get L3 table
		uint64_t *L3 = L2[L2_idx] & PTE_ADDR_MASK;

		// L3 entry
		if (mapping_layer == 3)
		{
			if ((L3[L3_idx] & 0b11) != (0b00))
			{
				// throw some error here!
				udbP("MMU WARNING! This virtual address is already mapped! E6");
				return;
			}
			// add L1 entry
			L3[L3_idx] = pa | flags | PTE_VALID | PTE_PAGE;
			remaining -= MMU_L3_MSIZE;
			va += MMU_L3_MSIZE;
			pa += MMU_L3_MSIZE;
			continue; // done! next!
		}
	}

	/*
	uart_print("mapped: pa: 0x");
	uart_print_hex64(saved_pa);
	uart_print(" - 0x");
	uart_print_hex64(saved_pa + ALIGN_UP(lenght, 4096));
	uart_print(" to va: 0x");
	uart_print_hex64(saved_va);
	uart_print(" - 0x");
	uart_print_hex64(saved_va + ALIGN_UP(lenght, 4096));
	uart_print("\n");
	*/

	return;
}

void mmu_unmap_range(uint64_t va, uint64_t length, mmu_context_t *ctx)
{
	uint64_t remaining = ALIGN_UP(length, 4096);

	uint64_t *L0 = nullptr;
	if (va >= UOS_TTBR1_MIN)
	{
		L0 = ctx->L0_ttbr1;
	}
	else
	{
		L0 = ctx->L0_ttbr0;
	}

	while (remaining)
	{
		uint64_t L0_idx = (va >> 39) & 0x1FF;
		uint64_t L1_idx = (va >> 30) & 0x1FF;
		uint64_t L2_idx = (va >> 21) & 0x1FF;
		uint64_t L3_idx = (va >> 12) & 0x1FF;

		// check L0
		if (L0 == nullptr)
		{
			// throw some error here!
			udbP("MMU WARNING! This virtual address is not mapped! E0 (L0 does not exsists");
			return;
		}

		// check L0 entry
		if (L0[L0_idx] & (0b11) != (PTE_VALID | PTE_PAGE))
		{
			// throw some error here!
			udbP("MMU WARNING! This virtual address is not mapped! E1 (L0 entry does not exsists");
			return;
		}

		uint64_t *L1 = L0[L0_idx] & PTE_ADDR_MASK;

		// check L1
		if (L1 == nullptr)
		{
			// throw some error here!
			udbP("MMU WARNING! This virtual address is not mapped! E2 (L1 does not exsists");
			return;
		}

		// check block or table
		if (L1[L1_idx] & (0b11) == (PTE_VALID | PTE_PAGE))
		{
			// table
			_mmu_destroy_node_(L1[L1_idx] & PTE_ADDR_MASK, ctx); // free node
		}
		else if (L1[L1_idx] & (0b11) == (PTE_VALID))
		{
			// block

			// check alignment & remaining size
			if (((va % MMU_L1_MSIZE) == 0) && remaining >= MMU_L1_MSIZE)
			{
				// destroy block
				L1[L1_idx] = 0x0; // remove entry
				remaining -= MMU_L1_MSIZE;
				va += MMU_L1_MSIZE;
				continue; // next!
			}
			else
			{
				// split block here

				// firstly destroy exsisting big block
				uint64_t old_flags = L1[L1_idx] & ~(PTE_ADDR_MASK | 0b11); // save flags for future
				L1[L1_idx] = 0x0;										   // remove entry

				// create L1 table
				PageTableNode *node = kmalloc(sizeof(PageTableNode)); // meta data
				node->next = nullptr;
				node->table = palloc(1);		// actual table 4kb aligned
				memset(node->table, 0x0, 4096); // zero all entries

				// add to L1 table
				L1[L1_idx] = ((uint64_t)node->table | (PTE_VALID | PTE_PAGE));

				// add to allocated list
				_mmu_ctx_add_node_(node, ctx);

				uint64_t freed_block_count = remaining % MMU_L1_MSIZE;

				// L2 blocks in L1 table
				uint64_t *new_L2 = L1[L1_idx] & PTE_ADDR_MASK;

				// fill not freed pages ()
				for (uint64_t i = freed_block_count; i < 512; i++)
				{
					new_L2[i] = 
				}
			}
		}
		else
		{
			// invalid
			// throw some error here!
			udbP("MMU WARNING! This virtual address is not mapped! E3 (L1 entry does not exsists");
			return;
		}
	}
}

void mmu_enable()
{
	uint64_t sctlr;

	asm volatile("mrs %0, sctlr_el1"
				 : "=r"(sctlr));
	sctlr |= (1ULL << 0);  // M
	sctlr |= (1ULL << 2);  // C
	sctlr |= (1ULL << 12); // I

	asm volatile("msr sctlr_el1, %0" ::"r"(sctlr));
	asm volatile("isb");
}

void mmu_disable()
{
	uint64_t sctlr;

	asm volatile(
		"mrs %0, sctlr_el1"
		: "=r"(sctlr));

	sctlr &= ~(1ULL << 0);	// M
	sctlr &= ~(1ULL << 2);	// C
	sctlr &= ~(1ULL << 12); // I

	asm volatile(
		"msr sctlr_el1, %0"
		:
		: "r"(sctlr));

	asm volatile("isb");
}

void mmu_set_ttbr0(void *root)
{
	uint64_t addr = (uint64_t)root;

	asm volatile(
		"msr ttbr0_el1, %0"
		:
		: "r"(addr));
}

void mmu_set_ttbr1(void *root)
{
	uint64_t addr = (uint64_t)root;

	asm volatile(
		"msr ttbr1_el1, %0"
		:
		: "r"(addr));
}

void mmu_switch_ttbr(mmu_context_t *ctx)
{
	mmu_set_ttbr0(ctx->L0_ttbr0);
	mmu_set_ttbr1(ctx->L0_ttbr1);

	//_mmu_dump_L0_(ctx->L0_ttbr0);
	//_mmu_dump_L0_(ctx->L0_ttbr1);
}

void mmu_barrier()
{
	asm volatile("dsb sy" ::
					 : "memory");
	asm volatile("isb" ::
					 : "memory");
}

void mmu_flush_tlb()
{
	asm volatile("tlbi vmalle1");
	asm volatile("dsb sy" ::
					 : "memory");
	asm volatile("isb" ::
					 : "memory");
}

// debug
void _mmu_dump_L0_(uint64_t *L0)
{
	uart_print("FULL table dump:\n");

	// L0
	for (int i = 0; i < 512; i++)
	{
		if ((L0[i] & 0b11) == 0b11)
		{
			uart_print_dec(i);
			uart_print(" is table (L0) pa: 0x");

			uint64_t *L1 = L0[i] & PTE_ADDR_MASK;

			uart_print_hex64(L1);
			uart_print("\n");

			// L1
			for (int j = 0; j < 512; j++)
			{
				if ((L1[j] & 0b11) == 0b11)
				{
					uart_print("-] ");
					uart_print_dec(j);
					uart_print(" is table (L1) pa: 0x");

					uint64_t *L2 = L1[j] & PTE_ADDR_MASK;

					uart_print_hex64(L2);
					uart_print("\n");

					// L2
					for (int k = 0; k < 512; k++)
					{
						if ((L2[k] & 0b11) == 0b11)
						{
							uart_print("--] ");
							uart_print_dec(k);
							uart_print(" is table (L2) pa: 0x");

							uint64_t *L3 = L2[k] & PTE_ADDR_MASK;

							uart_print_hex64(L3);
							uart_print("\n");

							// L3
							for (int b = 0; b < 512; b++)
							{
								if ((L3[b] & 0b11) == 0b11)
								{
									uart_print("---] ");
									uart_print_dec(b);
									uart_print(" is page (L3) pa: 0x");
									uart_print_hex64(L3[b] & PTE_ADDR_MASK);
									uart_print(" flags: 0x");
									uart_print_hex64(L3[b] & ~PTE_ADDR_MASK);
									uart_print("\n");
								}
							}
						}
						else if ((L2[k] & 0b11) == 0b01)
						{
							uart_print("--] ");
							uart_print_dec(k);
							uart_print(" is block (L2) pa: 0x");
							uart_print_hex64(L2[k] & PTE_ADDR_MASK);
							uart_print(" flags: 0x");
							uart_print_hex64(L2[k] & ~PTE_ADDR_MASK);
							uart_print("\n");
						}
					}
				}
				else if ((L1[j] & 0b11) == 0b01)
				{
					uart_print("-] ");
					uart_print_dec(j);
					uart_print(" is block (L1) pa: 0x");
					uart_print_hex64(L1[j] & PTE_ADDR_MASK);
					uart_print(" flags: 0x");
					uart_print_hex64(L1[j] & ~PTE_ADDR_MASK);
					uart_print("\n");
				}
			}
		}
	}
}