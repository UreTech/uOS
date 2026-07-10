#include <memory/u_mempage.h>

UMP_MREGION_HEADER *mmr_header = NULL;

UMP_MREGION_HEADER *vmmr_header = NULL;

void ump_allocator_init_mem()
{
	// first align memory bounds to pages
	mem = ALIGN_UP((uint64_t)mem, UMP_PAGE_SIZE);
	mem_end = ALIGN_DOWN((uint64_t)mem_end, UMP_PAGE_SIZE);

	// set mmr header to first page
	mmr_header = (UMP_MREGION_HEADER *)mem;
	mmr_header->_raw_first_page_ = (uint8_t *)mmr_header;

	// calculate total page count
	mmr_header->total_page_count = (mem_end - mem) / UMP_PAGE_SIZE;

	// calculate free table & data page count
	size_t onePageFreeTableBlock_pageCount = (UMP_PAGE_SIZE * 8) + 1;

	mmr_header->FPT_page_count = mmr_header->total_page_count / onePageFreeTableBlock_pageCount; // 1 bit per page

	if (mmr_header->total_page_count % onePageFreeTableBlock_pageCount)
	{
		mmr_header->FPT_page_count++;
	}

	mmr_header->DP_page_count = mmr_header->total_page_count - mmr_header->FPT_page_count - 1;

	uart_print("FPT page count: ");
	uart_print_dec(mmr_header->FPT_page_count);
	uart_print("\n");

	uart_print("DP count: ");
	uart_print_dec(mmr_header->DP_page_count);
	uart_print("\n");

	// set start of Free Page Table
	mmr_header->FPT = (uint8_t *)mem + sizeof(onepage);

	// set start of Data Pages
	mmr_header->DP_start = (uint8_t *)(mmr_header->FPT + mmr_header->FPT_page_count * sizeof(onepage));

	// reset free table
	for (u64 i = 0; i < mmr_header->FPT_page_count; i++)
	{
		for (u64 j = 0; j < UMP_PAGE_SIZE; j++)
		{
			mmr_header->FPT[(i * UMP_PAGE_SIZE) + j] = 0x00;
		}
	}

	// Done!
}

void ump_allocator_init_vmem()
{
	// first align memory bounds to pages
	vmem = ALIGN_UP((uint64_t)vmem, UMP_PAGE_SIZE);
	vmem_end = ALIGN_DOWN((uint64_t)vmem_end, UMP_PAGE_SIZE);

	// set mmr header to first page
	vmmr_header = (UMP_MREGION_HEADER *)vmem;
	vmmr_header->_raw_first_page_ = (uint8_t *)vmmr_header;

	// calculate total page count
	vmmr_header->total_page_count = (vmem_end - vmem) / UMP_PAGE_SIZE;

	// calculate free table & data page count
	size_t onePageFreeTableBlock_pageCount = (UMP_PAGE_SIZE * 8) + 1;

	vmmr_header->FPT_page_count = vmmr_header->total_page_count / onePageFreeTableBlock_pageCount; // 1 bit per page

	if (vmmr_header->total_page_count % onePageFreeTableBlock_pageCount)
	{
		vmmr_header->FPT_page_count++;
	}

	vmmr_header->DP_page_count = vmmr_header->total_page_count - vmmr_header->FPT_page_count - 1;

	uart_print("vFPT page count: ");
	uart_print_dec(vmmr_header->FPT_page_count);
	uart_print("\n");

	uart_print("vDP count: ");
	uart_print_dec(vmmr_header->DP_page_count);
	uart_print("\n");

	// set start of Free Page Table
	vmmr_header->FPT = (uint8_t *)vmem + sizeof(onepage);

	// set start of Data Pages
	vmmr_header->DP_start = (uint8_t *)(vmmr_header->FPT + vmmr_header->FPT_page_count * sizeof(onepage));

	// reset free table
	for (u64 i = 0; i < vmmr_header->FPT_page_count; i++)
	{
		for (u64 j = 0; j < UMP_PAGE_SIZE; j++)
		{
			vmmr_header->FPT[(i * UMP_PAGE_SIZE) + j] = 0x00;
		}
	}

	// Done!
}

void ump_FPT_set_page_status(uint64_t page, uint8_t status, UMP_MREGION_HEADER *mmrh)
{
	// find the byte containing page status
	uint64_t byte_offset = page / 64;
	uint8_t in_byte_offset = page % 64;

	if (status)
	{
		((uint64_t *)(mmrh->FPT))[byte_offset] |= ONEBIT(in_byte_offset); // set bit
	}
	else
	{
		((uint64_t *)(mmrh->FPT))[byte_offset] &= ~ONEBIT(in_byte_offset); // reset bit
	}
}

void ump_FPT_set_range_page_status(uint64_t page, uint64_t page_count, uint8_t status, UMP_MREGION_HEADER *mmrh)
{
	// ignore
	if (!page_count)
		return;

	uint64_t pages_left = page_count;
	while (pages_left)
	{
		uint64_t current_page = page + (page_count - pages_left);

		if (!(current_page % 64) && pages_left >= 64)
		{
			// full 64 page
			if (status)
			{
				((uint64_t *)(mmrh->FPT))[current_page / 64] = ~(0b0);
			}
			else
			{
				((uint64_t *)(mmrh->FPT))[current_page / 64] = (0b0);
			}
			pages_left -= 64;
		}
		else
		{
			//  half page
			if (status)
			{
				((uint64_t *)(mmrh->FPT))[current_page / 64] |= ONEBIT(current_page % 64); // set
			}
			else
			{
				((uint64_t *)(mmrh->FPT))[current_page / 64] &= ~ONEBIT(current_page % 64); // clear
			}
			pages_left--;
		}
	}
	// Done!
}

// helpers

int get_bit_with_index(uint8_t *buffer, uint64_t index)
{
	if (index != 0)
	{
		return (buffer[index / 8] >> (index % 8)) & 0b1;
	}
	else
	{
		return (*buffer) & 0b1;
	}
}

uint64_t lastFreePageIndex = 0;
uint64_t _ump_get_continous_free_pages_(size_t pageCount)
{
	uint64_t foundLenght = 0;
	uint64_t startIndex = lastFreePageIndex;

	for (uint64_t i = lastFreePageIndex; i < mmr_header->DP_page_count; i++)
	{
		if (get_bit_with_index(mmr_header->FPT, i) == UMP_FPT_FREE)
		{
			if (foundLenght)
			{
				foundLenght++;
			}
			else
			{
				startIndex = i;
				foundLenght++;
			}

			if (foundLenght == pageCount)
			{
				return startIndex;
			}
		}
		else
		{
			foundLenght = 0;
		}
	}

	// no continious pages  found
	return -1;
}

uint64_t lastFreeVideoPageIndex = 0;
uint64_t _ump_get_continous_free_video_pages_(size_t pageCount)
{
	uint64_t foundLenght = 0;
	uint64_t startIndex = lastFreeVideoPageIndex;

	for (uint64_t i = lastFreeVideoPageIndex; i < vmmr_header->DP_page_count; i++)
	{
		if (get_bit_with_index(vmmr_header->FPT, i) == UMP_FPT_FREE)
		{
			if (foundLenght)
			{
				foundLenght++;
			}
			else
			{
				startIndex = i;
				foundLenght++;
			}

			if (foundLenght == pageCount)
			{
				return startIndex;
			}
		}
		else
		{
			foundLenght = 0;
		}
	}

	// no continious pages  found
	return -1;
}

// mem
void *ump_palloc(uint64_t page_count)
{
	uint64_t found = _ump_get_continous_free_pages_(page_count);
	if (found == -1)
	{
		return nullptr;
	}

	ump_FPT_set_range_page_status(found, page_count, UMP_FPT_ALLOCATED, mmr_header);

	// try to find first free page for optimisation
	// must not bigger than already allocated area but we must guarantee
	if (lastFreePageIndex >= found)
	{
		lastFreePageIndex = found + page_count;
		lastFreePageIndex = _ump_get_continous_free_pages_(1);
	}

	mmr_header->allocatedPageCount += page_count;
	return (void *)(mmr_header->DP_start + (found * sizeof(onepage)));
}

// vmem
void *ump_vpalloc(uint64_t page_count)
{
	uint64_t found = _ump_get_continous_free_video_pages_(page_count);
	if (found == -1)
	{
		return nullptr;
	}

	ump_FPT_set_range_page_status(found, page_count, UMP_FPT_ALLOCATED, vmmr_header);

	// try to find first free page for optimisation
	// must not bigger than already allocated area but we must guarantee
	if (lastFreeVideoPageIndex >= found)
	{
		lastFreeVideoPageIndex = found + page_count;
		lastFreeVideoPageIndex = _ump_get_continous_free_video_pages_(1);
	}

	vmmr_header->allocatedPageCount += page_count;
	return (void *)(vmmr_header->DP_start + (found * sizeof(onepage)));
}

//  mem
void ump_pfree(void *firstPage, uint64_t page_count)
{
	if ((uint64_t)firstPage % sizeof(onepage))
	{
		udbP("UMP WARNING! Unaligned page free request is  not allowed!");
		return;
	}

	if (!page_count)
	{
		udbP("UMP WARNING! 0 page free request is  not allowed!");
		return;
	}

	if ((uint64_t)firstPage < (uint64_t)mmr_header->DP_start)
	{
		udbP("UMP WARNING! firstPage is out of range! (DOWN)");
		return;
	}

	uint64_t startPage = 0;
	if (((uint64_t)firstPage - (uint64_t)mmr_header->DP_start))
	{
		startPage = ((uint64_t)firstPage - (uint64_t)mmr_header->DP_start) / sizeof(onepage);
	}

	if (startPage + page_count > mmr_header->DP_page_count)
	{
		udbP("UMP WARNING! lastPage is out of range! (UP)");
		return;
	}

	//  set pages to free
	ump_FPT_set_range_page_status(startPage, page_count, UMP_FPT_FREE, mmr_header);

	// set as lastFreePage if its more closer to 0
	if (lastFreePageIndex > startPage)
	{
		lastFreePageIndex = startPage;
	}
	
	mmr_header->allocatedPageCount -= page_count;
	
}

//  vmem
void ump_vpfree(void *firstPage, uint64_t page_count)
{
	if ((uint64_t)firstPage % sizeof(onepage))
	{
		udbP("UMP WARNING! Unaligned video page free request is  not allowed!");
		return;
	}

	if (!page_count)
	{
		udbP("UMP WARNING! 0 video page free request is  not allowed!");
		return;
	}

	if ((uint64_t)firstPage < (uint64_t)vmmr_header->DP_start)
	{
		udbP("UMP WARNING! video firstPage is out of range! (DOWN)");
		return;
	}

	uint64_t startPage = 0;
	if (((uint64_t)firstPage - (uint64_t)vmmr_header->DP_start))
	{
		startPage = ((uint64_t)firstPage - (uint64_t)vmmr_header->DP_start) / sizeof(onepage);
	}

	if (startPage + page_count > vmmr_header->DP_page_count)
	{
		udbP("UMP WARNING! video lastPage is out of range! (UP)");
		return;
	}

	//  set pages to free
	ump_FPT_set_range_page_status(startPage, page_count, UMP_FPT_FREE, vmmr_header);

	// set as lastFreePage if its more closer to 0
	if (lastFreeVideoPageIndex > startPage)
	{
		lastFreeVideoPageIndex = startPage;
	}
	
	vmmr_header->allocatedPageCount -= page_count;
	
}

uint64_t ump_allocated_page_count(){
	return mmr_header->allocatedPageCount;
}

uint64_t ump_allocated_vpage_count(){
	return vmmr_header->allocatedPageCount;	
}