// Basic constants and macros
#define WSIZE	8	// Word and header/footer size (bytes)
#define DSIZE 	16	// long double wordsize
#define CHUNKSIZE (1<<12) // Extend the heap by multiple of this amount (2^12 = 4096)
#define MIN_BLOCK_SIZE 	((DSIZE) + (WSIZE) + (WSIZE))	// long double, header, footer

#define MAX(x, y) ((x) > (y) ? (x) : (y))

// Pack a size and allocated bit into a word
// // I need to make one for requested size, block size, and alloc
#define PACK(size, alloc) ((size) | (alloc))
//#define PACK(requested_size, block_size, alloc)

// Read and write a word at address P
#define GET(p) 		(*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

// Read the size and allocated fields from address p(p is header of footer)
#define GET_SIZE(p)		(GET(p) & ~0x7)	// gets 00a
#define GET_ALLOC(p)	(GET(p) & 0x1) // gets a
// also need to make a getrequested size

// Given block ptr bp, computer address of its header and footer (bp is what's passed into free())
#define GET_HEADER(bp)	((char*)(bp) - WSIZE)
#define GET_FOOTER(bp) 	((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// Given a block ptr bp, compute address of next and previous blocks 
#define NEXT_BLOCK(bp)	((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLOCK(bp)	((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

#define INC_HEAP() sbrk(CHUNKSIZE)

#define ASSERT_BOUNDARY(p) assert(p % 16 == 0)



// Use this code to compute the size of the next block in memory
size_t size = GET_SIZE(HDRP(NEXT_BLOCK(bp)));

/**
 * Gets four words from the memory system and initializes them to create 
 * the empty free list. It then calls the extend_heap function, which extends
 * the heap by CHUNKSIZE bytes and creates the initial free block.
 * At this point, the allocator is initialized and ready to accept
 * allocate and free requests.
 */
int init()
{
	// Create the intial empty heap
	heap_listp = mem_sbrk(4 * WSIZE));
	if (heap_listp == (void*) -1)
		return -1;

	PUT(heap_listp, 0);								// Alignment padding
	PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));	// Prologue header
	PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));	// Prologue footer
	PUT(heap_listp + (3 * WSIZE), PACK(0, 1));		// Epilogue header
	heap_listp += (2 * WSIZE);

	// Extend the empty heap with a free block of CHUNKSIZE bytes
	if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
		return -1;
	return 0;
}


/**
 * Invoked in two different circumstances:
 * 1. When the heap is initialized
 * 2. When malloc is unable to find a suitable fit.
 * To maintain alignment, extend_heap rounds UP the requested size to the nearest
 * multiple of 2 words and then requests the additional heap space from the
 * memory system.
 *
 * The heap begins on a double-word aligned boundary, and every call to extend_heap
 * returns a block whose size is an integral number of double words. Thus, every
 * call to sbrk() returns a double-word aligned chunk of memory immediately
 * following the header of the EPILOGUE block. This header becomes the header of the 
 * new free block, and the last word of the chunk becomes the new epilogue block header.
 *
 * Finally, in the likely case that the previous heap was terminated by a free block,
 * we call the coalesce function to merge the two free blocks and return the block
 * pointer of the merged blocks.
 */
static void *extend_heap(size_t words)
{
	char *bp;
	size_t size;

	// Allocate an even num of words to maintain alignment
	size = (words % 2) ? (words + 1)  * WSIZE : words * WSIZE;
	if ((long)(bp = sbrk(size)) == -1)
		return NULL;

	// Initialize the free block header/footer and the epilogue header
	PUT(GET_HEADER(bp), PACK(size, 0)); 			// Free block header
	PUT(GET_FOOTER(bp), PACK(size, 0));			// Free block footer
	PUT(GET_HEADER(NEXT_BLOCK(bp)), PACK(0,1));		// New epilogue header

	// Coalesce if the previous block was free
	return coalesce(bp);
}


/**
 * Frees a previously allocated block then merges adjacent free blocks using the 
 * footer coalescing technique. 
 */
void free(void *bp)
{
	size_t size = GET_SIZE(HDRP(bp));

	PUT(GET_HEADER(bp), PACK(size, 0));
	PUT(GET_FOOTER(bp), PACK(size, 0));

	coalesce(bp);
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
static void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(GET_FOOTER(PREV_BLOCK(bp)));
	size_t next_alloc = GET_ALLOC(GET_HEADER(NEXT_BLOCK(bp)));
	size_t size = GET_SIZE(GET_HEADER(bp));

	// CASE 1
	if (prev_alloc && next_alloc)
	{
		return bp;
	}

	// CASE 2 // next is free
	else if (prev_alloc && !next_alloc)
	{
		size += GET_SIZE(GET_HEADER(NEXT_BLOCK(bp)));
		PUT(GET_HEADER(bp), PACK(size, 0));
		PUT(GET_FOOTER(bp), PACK(size, 0));		
	}

	// CASE 3 // prev is free
	else if (!prev_alloc && next_alloc)
	{
		size += GET_SIZE(GET_HEADER(PREV_BLOCK(bp)));
		PUT(GET_FOOTER(bp), PACK(size, 0));
		PUT(GET_HEADER(PREV_BLOCK(bp)), PACK(size, 0));
		bp = PREV_BLOCK(bp);
	}

	// CASE 4
	else 
	{
		size += GET_SIZE(GET_HEADER(PREV_BLOCK(bp))) +
			GET_SIZE(GET_FOOTER(NEXT_BLOCK(bp)));
		PUT(GET_HEADER(PREV_BLOCK(bp)), PACK(size, 0));
		PUT(GET_FOOTER(NEXT_BLOCK(bp)), PACK(size, 0));
		bp = PREV_BLOCK(bp);
	}

	return bp;
}

/**
 * Request a block of size bytes of memory. After checking for spurious requests,
 * the allocator must adjust the requested block size to allow room for the header
 * and footer, and to satisfy the double-word alignment requirement. 
 *
 * We enforce the minimum block size of 24 bytes, 8 for the alignment requirement
 * 8 for header, and 8 for footer. 
 *
 * For requests over 8 bytes, the general rule is to add in the overhead bytes and
 * then round up to the nearest multiple of 8(16?).
 *
 * Once the allocator has adjusted the requested size, it searches the free list for
 * a suitable free block. If there is a fit, then the allocator places the requested
 * block and optionally splits the excess and then returns the address of the newly
 * allocated block. Mark the requested size in the header and footer as well.
 *
 * If the allocator cannot find a fit, it extends the heap with a new free block,
 * places the requested block in the new free block, optionally splitting the block,
 * and thenr eturns a pointer to the newly allocated block.
 *
 * Should only place the requested block at the beginning of the free block and
 * split if the size of the remainder would equal or exceed the minimum block size. (DSIZE?)
 *
 */
void *malloc(size_t size)
{
	size_t asize;	// Adjusted block size
	size_t extendsize; // Amount to extend heap if no fit.
	char *bp;

	// Ignore spurious requests
	if (size == 0)
		return NULL;

	// Adjust block size to include overhead and alignment reqs
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

	// Search the free list for a fit
	if (bp = find_fit(asize) != NULL)
	{
		place(bp, asize);
		return bp;
	}

	// No fit found. Get more memory and place the block.
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;
	place(bp, asize);
	return bp;
}

// first-fit search
static void *find_fit(size_t asize)
{

}

// place the requested block at the beginnign of the free block, splitting
// only if the size of the remainder would equal or exceed the minimum block size.
static void place(void * bp, size_t asize)
{

}