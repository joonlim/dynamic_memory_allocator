/**
 * mm.c
 */

/**
 * The allocator exports three functions to paplication programs
 * extern int mm_init(void);
 * extern void *mm_malloc(size_t size);
 * extern void mm-free(void *ptr);
 */

/* Private global variables */
static char *heap;	// Points to first byte of heap */
static char *brk;	// Points to last byte of heap + 1
static char *max_addr; // Max legal heap addr + 1

/**
 * Initialize the memory system model
 *
 */
void init()
{
	extend_heap(CHUNKSIZE);
	heap = (char*)Malloc(MAX_HEAP);
	brk = (char*)mem_heap;
	max_addr = (char*)(mem_heap + MAX_HEAP);
}

/**
 * Simple model of the sbrk function. Extends the heap by incr bytes an
 * returns the start address of the new area. In this model, the heap
 * cannot be shrunk.
 * @return previous top of heap
 */
void *sbrk(int incr)
{
	char *old_brk = brk;

	if ((incr < 0) || ((brk + incr) > max_addr))
	{
		errno = ENOMEM;
		fprintf(stderr, "ERROR: sbrk failed. Ran out of memory...\n");
		return (void =)-1;
	}

	brk += incr;
	return (void*)old_brk;
}