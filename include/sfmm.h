/**
 * In this code, block refers to the single smallest unit
 * and region refers to an allocated/free section of the heap.
 */

#ifndef __SFMM_H
#define __SFMM_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

/* Easy ints */
#define int8 uint8_t
#define int32 uint32_t
#define int64 uint64_t

#define FOUR_KB 4096
#define FOUR_GB 4294967296
#define MAX 4294967288

/* Basic constants and macros */
#define WSIZE 	8	// Word and header/footer size in Bytes
#define DSIZE	16	// long double size in Bytes
#define MIN_REGION_SIZE ((DSIZE) + (WSIZE) + (WSIZE)) // long double + header + footer

/* Given a requested size, return how big the region including padding, header, and footer should be */
#define REGION_SIZE(requested_size)		(((((requested_size - 1) / 16) + 1) * 16) + (2 * (WSIZE))) 

/* allocated bit a */
#define ALLOCATED 0x1
#define FREE 	  0x0

/* Pack the requested size, actual region size, and allocation bit into one word */
/* Use this to create the header/footer of a region */
/*
	|=================================|==============================|00a|	- 64 bit
			32-bit Requested Size 				29-bit Region size  	  	allocated bit
 */
#define PACK(requested_size, region_size, a) (((region_size) | (a)) | (((requested_size) << 16) << 16) )	

/* Given an arbitrary address, return the 64-bit word(block) at the address */
#define GET(p) 			(*(int64 *)(p))
/* Given an arbitrary address, write to it a 64-bit value */
#define PUT(p, val64)	(*(int64 *)(p) = (val64))

/* Given a pointer to header or footer hp, return the requested_size */
#define GET_REQUESTED_SIZE(hp)	(GET(hp) >> 32)
/* Given a header or footer h, return the actual region size */
#define GET_REGION_SIZE(hp)		(GET(hp) & 0xFFFFFFF8)
/* Given a header or footer h, return the allocated bit */
#define GET_ALLOC(hp) 			(GET(hp) & 0x1)		

/* Given an address to region rp, return address of header */
#define HEADER_ADDRESS(rp)	((char*)(rp) - WSIZE)
/* Given an address to region rp, return address of footer */
#define FOOTER_ADDRESS(rp)	((char*)(rp) + GET_REGION_SIZE(HEADER_ADDRESS(rp)) - DSIZE)
/* Given an address, return address of word after. Used to get the region pointer and the freelist pointer */ 
#define NEXT_WORD(p)		((char*)(p) + WSIZE)
/* Given an address, return address of word before. */
#define PREV_WORD(p)		((char*)(p) + WSIZE)

/* Given an address to region rp, compute address of next and previous regions */ 
#define NEXT_REGION(rp)	((char *)(rp) + GET_REGION_SIZE(((char *)(rp) - WSIZE)))
#define PREV_REGION(rp)	((char *)(rp) - GET_REGION_SIZE(((char *)(rp) - DSIZE)))

/* Given an address to region rp, return address of next header */
#define NEXT_HEADER_ADDRESS(rp) (FOOTER_ADDRESS(rp) + WSIZE)
/* Given an address to region rp, return address of prev header */
#define PREV_FOOTER_ADDRESS(rp) (HEADER_ADDRESS(rp) - WSIZE)

#define PREV_HEADER_ADDRESS(rp) HEADER_ADDRESS(rp) - GET_REGION_SIZE(PREV_FOOTER_ADDRESS(rp))
/* Given a header, compute the address of the back link and forward link, which point to back and forward free heads */
/* Only use these on free heads */
#define FORWARD_LINK(hp)	NEXT_WORD(hp)
#define BACK_LINK(hp)		NEXT_WORD(NEXT_WORD(hp))
/**
 * This routine will initialize your memory allocator. It is called the
 * `_start` function which is called before main is called.
 */
void sf_mem_init(void);

/**
 * This is your implementation of malloc. It creates dynamic memory which
 * is aligned and padded properly for the underlying system. This memory
 * is uninitialized.
 * @param size The number of bytes requested to be allocated.
 * @return If successful, the pointer to a valid region of memory
 * to use is returned, else the value NULL is returned and the
 * ERRNO is set accordingly. If size is set to zero, then the
 * value NULL is returned.
 */
void* sf_malloc(size_t size);

/** Marks a dynamically allocated region as no longer in use.
 * @param ptr Address of memory returned by the function sf_malloc,
 * sf_realloc, or sf_calloc.
 */
void sf_free(void *ptr);

/**
 * Resizes the memory pointed to by ptr to be size bytes.
 * @param ptr Address of the memory region to resize.
 * @param size The minimum size to resize the memory to.
 * @return If successful, the pointer to a valid region
 * of memory to use is returned, else the value NULL is
 * returned and the ERRNO is set accordingly.
 */
void* sf_realloc(void *ptr, size_t size);

// /**
//  * Allocate an array of nmemb elements each of size bytes.
//  * The memory returned is additionally zeroed out.
//  * @param nmemb Number of elements in the array.
//  * @param size The size of bytes of each element.
//  * @return If successful, returns the pointer to a valid
//  * region of memory to use, else the value NULL is returned
//  * and the ERRNO is set accordingly. If nmemb or
//  * size is set to zero, then the value NULL is returned.
//  */
// void* sf_calloc(size_t nmemb, size_t size);

// /**
//  * Function which outputs the state of the free-list to stdout.
//  * See sf_snapshot section for details on output format.
//  */
// void sf_snapshot(void);

#endif
