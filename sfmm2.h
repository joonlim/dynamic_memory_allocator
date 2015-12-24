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

/* Easy 64 bit int */
#define int64 uint64_t

#define FOUR_KB 4096
#define FOUR_GB 4294967296

/* Basic constants and macros */
#define WORD_SIZE	8	// How many Bytes a word contains (64 bits)
#define LONG_SIZE 	16	// How many Bytes a long double takes up
#
#define MIN_REGION_SIZE 4	// in words







#endif