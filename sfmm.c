#include "include/sfmm.h"

static int64 *prologue_header;
static int64 *prologue_footer;
static int64 *epilogue_header;

static int64 *heap_start;	// always points to the start of the heap
static size_t heap_size = 0;	// size of the heap

static int64 *freelist_pointer;	// pointer to head of start of explicit freelist
								// if ADDRESS is set to true, this always points to the first
								// free region in address order.

#ifdef NEXT
	static int64 *next_free_pointer; // pointer that points to the free region after the one we just allocated
#endif


/* private function declarations */
void print_heap_stats();
void print_all_regions();
void print_region_stats(void *ptr);
static void *extend_heap(size_t size);
static void *find_fit(size_t size);
static void *find_fit_recursive(void *fp, size_t size);
static void place(void *ptr, size_t adjusted_size, size_t requested_size);
static void *coalesce(void *ptr);
static bool is_valid_heap_ptr(void *ptr_to_free);
static void *allocate(size_t size);
#ifdef ADDRESS
	static void convert_to_address_policy();
#endif
void sf_mem_init()
{
	#ifdef DEBUG
		printf("Initialize memory management\n");
	#endif

	// Create the intial empty heap with enough space for front padding, prologue and epilogue blocks.
	heap_size += MIN_REGION_SIZE;
	heap_start = (int64 *)sbrk(heap_size);
	

	// Alignment padding set to 0x0
	PUT(heap_start, 0);								

	// Set prologue header. This never changes.
	prologue_header = (int64*)NEXT_WORD(heap_start);
	PUT(prologue_header, PACK(0, DSIZE, ALLOCATED));

	// Set prologue footer. This never changes.
	prologue_footer = (int64*)NEXT_WORD(prologue_header);
	PUT(prologue_footer, PACK(0, DSIZE, ALLOCATED));

	// Set epilogue header
	epilogue_header = (int64*)NEXT_WORD(prologue_footer);
	PUT(epilogue_header, PACK(0, 0, ALLOCATED));

	freelist_pointer = epilogue_header;
	
	#ifdef NEXT
		next_free_pointer = freelist_pointer;
	#endif

	#ifdef DEBUG
		print_heap_stats();
	#endif
}

void* sf_malloc(size_t size)
{
	errno = 0;

	#ifdef DEBUG
		printf("\nCall to malloc() - size: %lu", size);
	#endif

	// Ignore spurious requests
	if (size <= 0)
		return NULL;

	// Disallow large requests
	if (size > FOUR_GB)
	{
		errno = ENOMEM;
		return NULL;
	}

	return allocate(size);
}

static void* allocate(size_t size)
{

	size_t adjusted_size = REGION_SIZE(size);
	#ifdef DEBUG
		printf(" - adjusted to: %lu\n", adjusted_size);
	#endif

	// Search the free list for a fit
	// The first time malloc is called, this should return NULL.
	int64 *rp = (int64*)find_fit(adjusted_size);
	if (rp != NULL)
	{
	 	#ifdef NEXT
	 		// set next-fit pointer to the next link of rp
			next_free_pointer = (int64*)GET(FORWARD_LINK(HEADER_ADDRESS(rp)));
		#endif	

		place(rp, adjusted_size, size);
		return rp;
	}

	// No fit found. Get more memory and place the block.
	// For anything under 4 KB we can just increase the heap by 4 KB.
	// However, if the malloc request is over 4KB and we don't have a fit we must
	// increase it 
	int64 factor = adjusted_size / FOUR_KB;
	int64 inc_by = (factor + 1) * FOUR_KB;

	if (heap_size < FOUR_KB)
	{
		// this is the case where malloc is called for the first time.
		// the heap is only 4 words big.
		rp = (int64*)extend_heap(inc_by - heap_size);
	}
	else
	{
		rp = (int64*)extend_heap(inc_by);
	}

	if (rp == NULL)
	{
		errno = ENOMEM;
		return NULL;
	}

 	#ifdef NEXT
 		// set next-fit pointer to the next link of rp
		next_free_pointer = (int64*)GET(FORWARD_LINK(HEADER_ADDRESS(rp)));
	#endif

	place(rp, adjusted_size, size);
	return rp;
}

void sf_free(void *ptr)
{
	#ifdef DEBUG
		printf("\nCall to free - %p\n", ptr);
	#endif
	if (!is_valid_heap_ptr(ptr))
		return;

	// 1. Mark block as free
	// 2. Coalesce adjacent free blocks
	// 3. Insert free block into the free list.
	// 		LIFO policy
	// 			Insert freed block at the beginning of the free list
	// 		Address-ordered policy
	// 			Insert freed block so that free list blocks are always in address order
	int64* rp = (int64*)ptr;

	size_t size_to_free = GET_REGION_SIZE(HEADER_ADDRESS(rp));

	PUT(HEADER_ADDRESS(rp), PACK(0, size_to_free, FREE));
	PUT(FOOTER_ADDRESS(rp), PACK(0, size_to_free, FREE));

	coalesce(rp);
}

void* sf_realloc(void *ptr, size_t size)
{
	errno = 0;

	#ifdef DEBUG
		printf("\nCall to realloc() - size: %lu", size);
		printf("\nAddress: %p", ptr);
	#endif

	// Ignore spurious requests
	if (size <= 0)
		return NULL;

	// Disallow large requests
	if (size > FOUR_GB)
	{
		errno = ENOMEM;
		return NULL;
	}

	sf_free(ptr);
	return allocate(size);
}

void* sf_calloc(size_t nmemb, size_t size)
{
	errno = 0;

	#ifdef DEBUG
		printf("\nCall to calloc() - size: %lu", size);
	#endif

	// Ignore spurious requests
	if (nmemb <= 0 || size <= 0)
		return NULL;

	// Disallow large requests
	if (size * nmemb > FOUR_GB)
	{
		errno = ENOMEM;
		return NULL;
	}

	void* allocated_region = allocate(nmemb * size);

	// zero out the memory
	void* iter = allocated_region;
	void* footer = FOOTER_ADDRESS(allocated_region);
	while (iter != footer)
	{
		PUT(iter, 0x0);
		iter =  (char*)iter + WSIZE;
	}
	return allocated_region;
}

void sf_snapshot()
{
	// Make sure user requested for heap space.
	if (GET_ALLOC(freelist_pointer) == FREE)
	{
		printf("Explicit 8 %lu\n\n", heap_size);
		/*
			08/23/12 - 12:40AM		# use strftime
			0x00095040 8
			0x0010a058 16
		 */
		
		char buffer[256];	
  		time_t curtime;
  		struct tm *loctime;

		/* Get the current time. */
		curtime = time(NULL);

		/* Convert it to local time representation. */
		loctime = localtime (&curtime);

   		strftime (buffer, 256, "# %m/%d/%y - %I:%M%p\n", loctime);
		printf("%s\n", buffer);

		void* ptr = freelist_pointer; // head of first free region
		printf("%p %lu\n", ptr, GET_REGION_SIZE(ptr));
		ptr = (void*)GET(FORWARD_LINK(ptr));

		while (ptr != freelist_pointer)
		{
			printf("%p %lu\n", ptr, GET_REGION_SIZE(ptr));
			ptr = (void*)GET(FORWARD_LINK(ptr));
		}

	}
}

/**
 * Print information about heap when debugging.
 */
void print_heap_stats()
{
	printf("\nCURRENT HEAP STATS\n");
	printf("heap_start: %p\n", heap_start);
	printf("prologue_header: %p - %lu\n", prologue_header, GET(prologue_header));
	printf("prologue_footer: %p - %lu\n", prologue_footer, GET(prologue_footer));
	printf("epilogue_header: %p - %lu\n", epilogue_header, GET(epilogue_header));
	printf("heap_size: %lu\n", heap_size);
	printf("freelist_pointer: %p\n", freelist_pointer);
	#ifdef NEXT
		printf("next_free_pointer: %p\n", next_free_pointer);
	#else
		printf("\n");
	#endif
}

/**
 * Increases heap by size if there is enough memory.
 * @return the pointer to the now newly acquired free region
 */
static void *extend_heap(size_t size)
{
	if (heap_size + size > MAX)
	{
		errno = ENOMEM;
		return NULL;
	}
	
	int64* rp;
	rp = (int64*)sbrk(size);

	heap_size += size;
	
	#ifdef DEBUG
		printf("extending heap size to: %lu\n", heap_size);
		printf("previous top of heap: %p\n", rp);
		printf("new top of heap: %p\n", sbrk(0));
	#endif

	// Initialize the free block header/footer of the free region
	PUT(HEADER_ADDRESS(rp), PACK(0, size, FREE)); 
	PUT(FOOTER_ADDRESS(rp), PACK(0, size, FREE));	

	// New epilogue header
	epilogue_header = (int64*)NEXT_HEADER_ADDRESS(rp);
	PUT(epilogue_header, PACK(0, 0, ALLOCATED));

	// If this is the first time we call malloc, indicated by heap_size:
	// Create the first link for the free list - a circular link back to itself.
	if (heap_size == FOUR_KB)
	{
		#ifdef DEBUG
			printf("First extension of heap. Initializing free list.\n");
		#endif

		// circular link to inicate only 1 free region at the moment
		PUT(FORWARD_LINK(freelist_pointer), (int64)freelist_pointer);
		PUT(BACK_LINK(freelist_pointer), (int64)freelist_pointer);
	}

	// Coalesce if the previous block was free
	return coalesce(rp);
}


/**
 * Find a free region that fits the size.
 * This function uses a explicit freelist using either first-fit or next-fit 
 * depending on the given flag.
 */
static void *find_fit(size_t size)
{
	// This is the case where there are no free regions.
	if (GET_ALLOC(freelist_pointer) == ALLOCATED)
	{
		#ifdef DEBUG
			printf("No free regions. We must extend the heap.\n");
		#endif
		return NULL;
	}

	int64* start_ptr;
	#ifdef NEXT
		start_ptr = next_free_pointer;
	#else
		start_ptr = freelist_pointer;
	#endif

	if (GET_REGION_SIZE(start_ptr) >= size)
	{
		#ifdef DEBUG
			printf("Free region found at first address! - %p.\n", start_ptr);
			printf("This region has a size of %lu.\n", GET_REGION_SIZE(start_ptr));
		#endif
		return NEXT_WORD(start_ptr);
	}

	// recursively check every free pointer after the start_ptr recursively.
	// Pass the address pointed to by start_ptr
	int64* next_address = (int64*)(*(int64*)FORWARD_LINK(start_ptr));
	return find_fit_recursive(next_address, size);


	// Next placement strategy
	// keep track of where previous search finished. 
	// then check in order until we find a big enough space.

}

/**
 * Iterate through a circular linked list to find a region of size size
 * @param fp header of a free region
 */
static void *find_fit_recursive(void *fp, size_t size)
{
	#ifdef DEBUG
		printf("\nSearching for free region recursively...");
	#endif

	int64* start_ptr;
	#ifdef NEXT
		start_ptr = next_free_pointer;
	#else
		start_ptr = freelist_pointer;
	#endif

	// BASE CASE: stop when we reach start_ptr again.
	if (fp == start_ptr) {
		#ifdef DEBUG
			printf("No free regions of size %lu.\n", size);
		#endif
		return NULL;
	}

	int64 region_size = GET_REGION_SIZE(fp);

	#ifdef DEBUG
		printf("Checking %p.\n", fp);
		printf("This region has a size of %lu.\n", region_size);
	#endif

	if (region_size >= size)
	{
		#ifdef DEBUG
			printf("Free region found at address! - %p.\n", fp);
			printf("\n");
		#endif

		return NEXT_WORD(fp);
	}

	int64* next_address = (int64*)(*(int64*)FORWARD_LINK(fp));
	return find_fit_recursive(next_address, size);

}

/**
 * Place the requested region at the beginning of the free region, splitting
 * only if the size of the remainder would equal or exceed the minimum block size.
 * ptr MUST be a free region.
 */
static void place(void *ptr, size_t adjusted_size, size_t requested_size)
{
	if (GET_ALLOC(ptr) != FREE)
	{
		#ifdef DEBUG
			printf("ERROR. CALLING place() ON ALREADY ALLOCATED REGION!\n\n");
		#endif
		return;
	}

	int64* rp = (int64*)ptr;

	int64 free_region_size = GET_REGION_SIZE(HEADER_ADDRESS(rp)); // size of the current free region

	if (free_region_size < adjusted_size)
	{
		#ifdef DEBUG
			printf("ERROR. CALLING place() ON REGION THAT IS TOO SMALL - %lu bytes\n\n", free_region_size);
		#endif
		return;
	}

	if (free_region_size > adjusted_size + (2 * WSIZE))
	{
		// split
		// ex:
		// size = 32, free_region_size = 48
		/*
		 __H_ ____ ____ ____ ____ __F_
		|    |    |    |    |    |    |
		| 48 | rp |    |    |    | 48 |
		 ---- ---- ---- ---- ---- ----
		 Free                     Free

		 convert to

		 __H_ ____ ____ __F_ __H_ __F_
		|    |    |    |    |    |    |
		| 32 | rp |    | 32 | 16 | 16 |
		 ---- ---- ---- ---- ---- ----
		 Allo     		Allo Free Free

		DO NOT split in cases like this b/c we have no space for forward and back links.
		 */

		int64 split_size = free_region_size - adjusted_size;

		#ifdef DEBUG
			printf("Splitting region into two of size %lu and %lu.\n\n", adjusted_size, split_size);
		#endif

		// Our allocated region
		PUT(HEADER_ADDRESS(rp), PACK(requested_size, adjusted_size, ALLOCATED));
		PUT(FOOTER_ADDRESS(rp), PACK(requested_size, adjusted_size, ALLOCATED));

		void* split_head = NEXT_HEADER_ADDRESS(rp);
		// split region
		PUT(split_head, PACK(0, split_size, FREE));
		PUT(FOOTER_ADDRESS(NEXT_WORD(split_head)), PACK(0, split_size, FREE));

		if (GET_ALLOC(freelist_pointer) == ALLOCATED )
			freelist_pointer = (int64*)GET(FORWARD_LINK(freelist_pointer));

		// set freelist_pointer if this is the first time we are allocating memory.
		if (GET_ALLOC(freelist_pointer) == ALLOCATED )
		{
			freelist_pointer = (int64*)split_head;

			PUT(BACK_LINK(split_head), (int64)split_head);
			PUT(FORWARD_LINK(split_head), (int64)split_head);

			#ifdef NEXT
				next_free_pointer = freelist_pointer;
			#endif

			return;
		}

		// Take care of forward and back links.
		void* split_forward_link = FORWARD_LINK(split_head);
		void* split_back_link = BACK_LINK(split_head);

		void* after = (void*)GET(FORWARD_LINK(HEADER_ADDRESS(rp)));
		void* before = (void*)GET(BACK_LINK(HEADER_ADDRESS(rp)));


		PUT(BACK_LINK(after), (int64)split_head);

		PUT(FORWARD_LINK(before), (int64)split_head);

		PUT(split_forward_link, (int64)after);
		PUT(split_back_link, (int64)before);

		if (freelist_pointer == (int64*)HEADER_ADDRESS(rp))
			freelist_pointer = (int64*)split_head;

		#ifdef NEXT
			if (next_free_pointer == (int64*)HEADER_ADDRESS(rp))
				next_free_pointer = freelist_pointer;
		#endif

		return;
	} 
	else if (free_region_size > adjusted_size )
	{
		// If we chose not to split because the resulting region would be too small,
		// we must include the size in the adjusted size.
		adjusted_size = adjusted_size + (2 * WSIZE);
	}

	PUT(HEADER_ADDRESS(rp), PACK(requested_size, adjusted_size, ALLOCATED));
	PUT(FOOTER_ADDRESS(rp), PACK(requested_size, adjusted_size, ALLOCATED));

	// Take care of forward and back links
	void * head = HEADER_ADDRESS(rp);
	void* forward_link_head = FORWARD_LINK(head);
	void* back_link_head = BACK_LINK(head);

	// connect the back and forward links with each other.
	PUT(BACK_LINK(GET(forward_link_head)), (int64)GET(back_link_head));
	PUT(FORWARD_LINK(GET(back_link_head)), (int64)GET(forward_link_head));

	if (freelist_pointer == (int64*)HEADER_ADDRESS(rp))
		freelist_pointer = (int64*)GET(forward_link_head);

	#ifdef NEXT
		if (next_free_pointer == (int64*)HEADER_ADDRESS(rp))
			next_free_pointer = freelist_pointer;
	#endif
}

/**
 * Help function with four cases. 
 * CASE 1: next and prev are allocated
 * CASE 2: next is free, prev is allocated
 * CASE 3: prev is free, prev is allocated
 * CASE 4: next and prev are free
 *
 * One subtle aspect: The prologue and epilogue allow us to ignore the potentially
 * troublesome edge conditions where the requested block bp is at the beginning
 * or end of the heap. Without these special blocks, the code would be messier,
 * more error prone, and slower.
 * 
 */
static void *coalesce(void *ptr)
{
	int64* rp = (int64*)ptr;

	size_t is_prev_alloc = GET_ALLOC(PREV_FOOTER_ADDRESS(rp));
	size_t is_next_alloc = GET_ALLOC(NEXT_HEADER_ADDRESS(rp));
	size_t size = GET_REGION_SIZE(HEADER_ADDRESS(rp));

	void* next_header = NEXT_HEADER_ADDRESS(rp);
	void* prev_header = PREV_HEADER_ADDRESS(rp);

	// CASE 1
	if (is_prev_alloc && is_next_alloc)
	{
		#ifdef DEBUG
			printf("coalescing case 1\n");
		#endif
		/*
		 ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____
		|    |    |    |    |    |    |	   |    |    |    |    |    |
		|    |    |    | F  | H  | rp |    | F  | H  |    |    |    |
		 ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		 				allo free           free allo

 		*/
 		// LIFO
 		// 
 		void* after = freelist_pointer;
 		if (after == (int64*)HEADER_ADDRESS(rp) || after == next_header)
 			after = (void*)GET(FORWARD_LINK(freelist_pointer));

 		void* before = (void*) GET(BACK_LINK(after));

 		freelist_pointer = (int64*)HEADER_ADDRESS(rp);

 		if (after != next_header)
 			PUT(FORWARD_LINK(freelist_pointer), (int64)after);
 		if (before != next_header)
 			PUT(BACK_LINK(freelist_pointer), (int64)before);

 		PUT(FORWARD_LINK(before), (int64)freelist_pointer);
 		PUT(BACK_LINK(after), (int64)freelist_pointer);

 		#ifdef NEXT
 			// if the next-fit pointer is pointing to an allocated region 
	 		// (because there were no free regions before)
	 		// then also reset it to be rp.
	 		if (GET_ALLOC(next_free_pointer) == ALLOCATED)
	 			next_free_pointer = (void*)HEADER_ADDRESS(rp);
		#endif 	
	}

	// CASE 2
	else if (is_prev_alloc && !is_next_alloc)
	{
		#ifdef DEBUG
			printf("coalescing case 2\n");
		#endif
		/*
		 ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____
		|    |    |    |    |    |    |	   |    |    |    |    |    |
		|    |    |    | F  | H  | rp |    | F  | H  |    |    |    |
		 ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		 				allo free           free free

 		*/
		size += GET_REGION_SIZE(NEXT_HEADER_ADDRESS(rp));
		PUT(HEADER_ADDRESS(rp), PACK(0, size, FREE));
		PUT(FOOTER_ADDRESS(rp), PACK(0, size, FREE));

 		// LIFO
 		// 
 		void* after = (void*) GET(FORWARD_LINK(next_header));
 		void* before = (void*) GET(BACK_LINK(next_header));

 		PUT(FORWARD_LINK(before), (int64)after);
 		PUT(BACK_LINK(after), (int64)before);

		//
 		after = freelist_pointer;
 		if (after == (int64*)HEADER_ADDRESS(rp) || after == next_header)
 			after = (void*)GET(FORWARD_LINK(freelist_pointer));

 		before = (void*) GET(BACK_LINK(after));

 		freelist_pointer = (int64*)HEADER_ADDRESS(rp);

 		if (after != next_header)
 			PUT(FORWARD_LINK(freelist_pointer), (int64)after);
 		if (before != next_header)
 			PUT(BACK_LINK(freelist_pointer), (int64)before);

 		PUT(FORWARD_LINK(before), (int64)freelist_pointer);
 		PUT(BACK_LINK(after), (int64)freelist_pointer);

	 	#ifdef NEXT
	 		// if the next-fit pointer is coalesced into rp, we reset it to be rp.
			if (next_free_pointer == next_header)	
				next_free_pointer = (void*)HEADER_ADDRESS(rp);
			// if the next-fit pointer is pointing to an allocated region 
	 		// (because there were no free regions before)
	 		// then also reset it to be rp.
	 		if (GET_ALLOC(next_free_pointer) == ALLOCATED)
	 			next_free_pointer = (void*)HEADER_ADDRESS(rp);
		#endif 		
	}
	
	// CASE 3
	else if (!is_prev_alloc && is_next_alloc)
	{
		#ifdef DEBUG
			printf("coalescing case 3\n");
		#endif
		/*
		 ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____
		|    |    |    |    |    |    |	   |    |    |    |    |    |
		|    |    |    | F  | H  | rp |    | F  | H  |    |    |    |
		 ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		 				free free           free allo

 		*/
		size += GET_REGION_SIZE(PREV_FOOTER_ADDRESS(rp));
		PUT(FOOTER_ADDRESS(rp), PACK(0, size, FREE));
		PUT(HEADER_ADDRESS(PREV_REGION(rp)), PACK(0, size, FREE));
		rp = (int64*)PREV_REGION(rp);

 		// LIFO
 		void* after = (void*) GET(FORWARD_LINK(prev_header));
 		void* before = (void*) GET(BACK_LINK(prev_header));

 		PUT(FORWARD_LINK(before), (int64)after);
 		PUT(BACK_LINK(after), (int64)before);

 		//

 		after = freelist_pointer;
 		if (after == (int64*)HEADER_ADDRESS(rp) || after == next_header)
 			after = (void*)GET(FORWARD_LINK(freelist_pointer));

 		before = (void*) GET(BACK_LINK(after));

 		freelist_pointer = (int64*)HEADER_ADDRESS(rp);

 		if (after != next_header)
 			PUT(FORWARD_LINK(freelist_pointer), (int64)after);
 		if (before != next_header)
 			PUT(BACK_LINK(freelist_pointer), (int64)before);

 		PUT(FORWARD_LINK(before), (int64)freelist_pointer);
 		PUT(BACK_LINK(after), (int64)freelist_pointer);

 		#ifdef NEXT
 			// if the next-fit pointer is pointing to an allocated region 
	 		// (because there were no free regions before)
	 		// then also reset it to be rp.
	 		if (GET_ALLOC(next_free_pointer) == ALLOCATED)
	 			next_free_pointer = (void*)HEADER_ADDRESS(rp);
		#endif 		
	}

	// CASE 4
	else
	{
		#ifdef DEBUG
			printf("coalescing case 4\n");
		#endif
		/*
		 ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____
		|    |    |    |    |    |    |	   |    |    |    |    |    |
		|    |    |    | F  | H  | rp |    | F  | H  |    |    |    |
		 ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		 				free free           free free

 		*/
		size += GET_REGION_SIZE(PREV_FOOTER_ADDRESS(rp)) + GET_REGION_SIZE(NEXT_HEADER_ADDRESS(rp));
		PUT(HEADER_ADDRESS(PREV_REGION(rp)), PACK(0, size, FREE));
		PUT(FOOTER_ADDRESS(NEXT_REGION(rp)), PACK(0, size, FREE));
		rp = (int64*)PREV_REGION(rp);

 		// LIFO
 		// 
 		void* after = (void*) GET(FORWARD_LINK(next_header));
 		void* before = (void*) GET(BACK_LINK(next_header));

 		PUT(FORWARD_LINK(before), (int64)after);
 		PUT(BACK_LINK(after), (int64)before);

 		//
 		//
 		after = (void*) GET(FORWARD_LINK(prev_header));
 		before = (void*) GET(BACK_LINK(prev_header));

 		PUT(FORWARD_LINK(before), (int64)after);
 		PUT(BACK_LINK(after), (int64)before);
 		//
 		
 		after = freelist_pointer;
 		if (after == (int64*)HEADER_ADDRESS(rp) || after == next_header)
 			after = (void*)GET(FORWARD_LINK(freelist_pointer));

 		before = (void*) GET(BACK_LINK(after));

 		freelist_pointer = (int64*)HEADER_ADDRESS(rp);

 		if (after != next_header)
 			PUT(FORWARD_LINK(freelist_pointer), (int64)after);
 		if (before != next_header)
 			PUT(BACK_LINK(freelist_pointer), (int64)before);

 		PUT(FORWARD_LINK(before), (int64)freelist_pointer);
 		PUT(BACK_LINK(after), (int64)freelist_pointer);

	 	#ifdef NEXT
	 		// if the next-fit pointer is coalesced into rp, we reset it to be rp.
			if (next_free_pointer == next_header)
				next_free_pointer = (void*)HEADER_ADDRESS(rp);
 			// if the next-fit pointer is pointing to an allocated region 
	 		// (because there were no free regions before)
	 		// then also reset it to be rp.
	 		if (GET_ALLOC(next_free_pointer) == ALLOCATED)
	 			next_free_pointer = (void*)HEADER_ADDRESS(rp);	
		#endif		
	}

	// easy fix hack
	#ifdef ADDRESS
		convert_to_address_policy();
	#endif

	return rp;
}

void print_all_regions()
{
	printf("\nPRINTING ALL REGION\n");
	printf("--------------------");

	int64* ptr = prologue_footer;
	ptr = (int64*)NEXT_WORD(ptr);
	while(ptr != epilogue_header)
	{
		print_region_stats(NEXT_WORD(ptr));
		ptr = (int64*)NEXT_HEADER_ADDRESS(NEXT_WORD(ptr));
	}
	printf("--------------------\n");
}

/**
 * Debugging method to print details about a region given its pointer.
 */
void print_region_stats(void *ptr)
{
	int64* rp = (int64*)ptr;
	printf("\nRegion %p", HEADER_ADDRESS(rp));

	if (rp == NULL)
	{
		printf("\n");
		return;
	}

	int64* header = (int64*)HEADER_ADDRESS(rp);
	int64* footer = (int64*)FOOTER_ADDRESS(rp);
	size_t is_alloc = GET_ALLOC(header);

	printf("-%p", footer);

	if (is_alloc)
	{
		printf(" - ALLOCATED\n");
	}
	else
	{
		printf(" - FREE - points to %p points from %p\n", (void*)GET(FORWARD_LINK(header)), (void*)GET(BACK_LINK(header)));
	}

	int64 requested_size = GET_REQUESTED_SIZE(header);
	int64 region_size = GET_REGION_SIZE(header);

	if (requested_size != GET_REQUESTED_SIZE(footer) || region_size != GET_REGION_SIZE(footer))
	{
		printf("ERROR. header and footer are not equal!\n");
		printf("header requested size: %lu\n", GET_REQUESTED_SIZE(header));
		printf("header region size: %lu\n", GET_REGION_SIZE(header));
		printf("footer requested size: %lu\n", GET_REQUESTED_SIZE(footer));
		printf("footer region size: %lu\n", GET_REGION_SIZE(footer));
	}

	else
	{
		printf("requested size: %lu\n", GET_REQUESTED_SIZE(header));
		printf("region size: %lu\n", GET_REGION_SIZE(header));
	}

}

static bool is_valid_heap_ptr(void* ptr_to_free)
{
	// 	- if ptr = NULL, return out
	// 	- if ptr = middle of region, return out
	// 	- if ptr = area not in heap, return out
	if (ptr_to_free == NULL || ptr_to_free <= (void*)prologue_footer || ptr_to_free >= (void*)epilogue_header)
	{
		#ifdef DEBUG
			printf("invalid pointer! cannot free! - %p\n", ptr_to_free);
		#endif
		return false;
	}

	int64* ptr = prologue_footer;
	ptr = (int64*)NEXT_WORD(ptr);
	while(ptr != epilogue_header)
	{
		if ((void*)NEXT_WORD(ptr) == ptr_to_free)
		{
			#ifdef DEBUG
				printf("Valid pointer! - %p\n", ptr_to_free);
			#endif
			return true;
		}
		ptr = (int64*)NEXT_HEADER_ADDRESS(NEXT_WORD(ptr));
	}

	#ifdef DEBUG
		printf("invalid pointer! cannot free! - %p\n", ptr_to_free);
	#endif
	return false;
}

#ifdef ADDRESS

/**
 * Converts the freelist into an address policy list.
 * 
 */
static void convert_to_address_policy()
{
	// Start
	void *iter_head = NEXT_WORD(prologue_footer);

	// 1. find the first free region and set it to freelist_pointer
	// iterate through every region until we find a free region
	while (iter_head != epilogue_header && GET_ALLOC(iter_head) != FREE)
	{
		iter_head = NEXT_HEADER_ADDRESS(NEXT_WORD(iter_head));
	}

	// found the first free region
	freelist_pointer = iter_head;

	if (iter_head == epilogue_header)
	{
		#ifdef DEBUG
			printf("No free regions when converting to address policy.\n");
		#endif
		return;
	}
	
	// 2. continuously iterate through every region.
	// if free, link to prev region
	void *prev_head = iter_head; // keep track of previous region so we can link them.
	iter_head = NEXT_HEADER_ADDRESS(NEXT_WORD(iter_head)); // next region
	while (iter_head != epilogue_header)
	{
		if (GET_ALLOC(iter_head) == FREE)
		{
			// found a free region! link them
			PUT(FORWARD_LINK(prev_head), (int64)iter_head);
			PUT(BACK_LINK(iter_head), (int64)prev_head);
			prev_head = iter_head;
		}
		iter_head = NEXT_HEADER_ADDRESS(NEXT_WORD(iter_head));
	}
	
	// 3. once we reach the end, link the last prev_head with freelist_pointer
	PUT(FORWARD_LINK(prev_head), (int64)freelist_pointer);
	PUT(BACK_LINK(freelist_pointer), (int64)prev_head);

}

#endif