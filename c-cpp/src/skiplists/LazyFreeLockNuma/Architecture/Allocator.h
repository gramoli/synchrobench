/*
 * Allocator.h: custom allocator for search layers
 *
 * Source: Henry Daly, 2018
 * Modified by: Thomas Salemy, 2018, for Lazy Lock Based implementation in C
 * 
 * Module Overview:
 *
 * This is a custom allocator to process allocation requests for NUMASK. It services
 * index and intermediate layer node allocation requests. We deploy one instance per NUMA
 * zone. The inherent latency of the OS call in numa_alloc_local (it mmaps per request)
 * practically requires these. Our allocator consists of a linear allocator with three
 * main alterations:
 * 		- it can reallocate buffers, if necessary
 *  	- allocations are made in a specific NUMA zone
 *		- requests are custom aligned for index and intermediate nodes to fit cache lines
 *
 * A basic linear allocator works as follows: upon initialization, a buffer is allocated.
 * As allocations are requested, the pointer to the first free space is moved forward and
 * the old value is returned.
 * 
 */

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stdlib.h>
#include <assert.h>

#define CACHE_LINE_SIZE 64

typedef struct numa_allocator {
	void*		buf_start;
	unsigned	buf_size;
	void*		buf_cur;
	char		empty;
	void*		buf_old;
	unsigned	cache_size;
	//for keeping track of the number of buffers
	void**		other_buffers;
	unsigned	num_buffers;
	//for half cache line alignment
	char		last_alloc_half;
} numa_allocator_t;

//constructors and destructors
numa_allocator_t* constructAllocator(unsigned ssize);
void destructAllocator(numa_allocator_t* allocator, unsigned ssize);

//driver methods
void* nalloc(numa_allocator_t* allocator, unsigned ssize);
void nfree(numa_allocator_t* allocator, void *ptr, unsigned ssize);


#endif 