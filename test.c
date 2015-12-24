/**
 * Tests
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


typedef int64_t ptr;

/**
 * find first free block that fits using first fit technique
 */
ptr first_fit()
{
	p = start;
	printf("start: %d\n", p);

	while((p < end)) &&					\
		  ((*p & 1)||(*p <= len))		\
		{
			p = p+ (*p & -2);
			printf("p: %d\n", p);
		}
		
}


void addblock(ptr p, int len)
{
	printf("p: %d, len: %d\n". p, len);

	int newsize = ((len + 1)) >> 1) << 1;
	printf("newsize: %d\n", newsize);

	// -2 = 1111 1111 1111 1110
	int oldsize = *p & -2; // use int64_t neg_two = -2
	printf("oldsize: %d\n", oldsize);

	*p = newsize | 1;
	printf("*p: %d\n", newsize);

	if (newsize < oldsize)
		*(p + newsize) = oldsize - newsize;
	printf("p + newsize: %d, *(p+newsize): %d\n", p+newsize, *(p+newsize));
}

/**
 * one direction coalescing
 */
void free_block(ptr p) {
	printf("p: %d\n", p);
	printf("*p: %d\n", *p);
	// -2 = 1111 1111 1111 1110
	*p = *p & -2; // use int64_t neg_two = -2
	printf("*p & -2: %d\n", *p);

	next = p + *p;
	printf("next: %d\n", next);
	if ((*next & 1) == 0)
	{
		*p = *p + *next;
		printf("*p: %d\n", *p);
	}
}