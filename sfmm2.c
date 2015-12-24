#include "sfmm2.h"

int64 num_words_in_region(int64 requested_size);

/**
 * Given a requested size through malloc, return the number of blocks
 * this region will take up.
 */
int64 num_words_in_region(int64 requested_size)
{
	/*
		1-16	-> 	1 -> 4
		17-32	->  2 -> 6
		33-48	->  3 -> 8
	 ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____
	|    |    |    |    |    |    |	   |    |    |    |    |    |
	|    |  H |    |    |    |    |  F |    |    |    |    |    |
	 ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	*/

	 return (((requested_size - 1) / LONG_SIZE) + 2) * 2;


}