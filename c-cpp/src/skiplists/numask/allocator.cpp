/*
 * allocator.cpp: custom allocator for search layers
 *
 * Author: Henry Daly, 2018
 *
 */

/**
 * Module Overview:
 *
 *	This is a custom allocator to process allocation requests for NUMASK. It services
 *	index and intermediate layer node allocation requests. We deploy one instance per NUMA
 *	zone. The inherent latency of the OS call in numa_alloc_local (it mmaps per request)
 *	practically requires these. Our allocator consists of a linear allocator with three
 *	main alterations:
 *		- it can reallocate buffers, if necessary
 *		- allocations are made in a specific NUMA zone
 *		- requests are custom aligned for index and intermediate nodes to fit cache lines
 *
 *	A basic linear allocator works as follows: upon initialization, a buffer is allocated.
 *	As allocations are requested, the pointer to the first free space is moved forward and
 *	the old value is returned.
 */

#include <stdio.h>
#include <numa.h>
#include "allocator.h"
#include "common.h"

/* Constructor */
numa_allocator::numa_allocator(unsigned ssize)
	:buf_size(ssize), empty(false), num_buffers(0), buf_old(NULL),
	 other_buffers(NULL), last_alloc_half(false), cache_size(CACHE_LINE_SIZE)
{
	buf_cur = buf_start = numa_alloc_local(buf_size);
}

/* Destructor */
numa_allocator::~numa_allocator() {
	// free all the buffers
	nreset();
}

/* nalloc() - service allocation request */
void* numa_allocator::nalloc(unsigned ssize) {
	// get cache-line alignment for request
	int alignment = (ssize <= cache_size / 2)? cache_size / 2: cache_size;

	/* if the last allocation was half a cache line and we want a full cache line, we move
	   the free space pointer forward a half cache line so we don't spill over cache lines */
	if(last_alloc_half && (alignment == cache_size)) {
		buf_cur = (char*)buf_cur + (cache_size / 2);
		last_alloc_half = false;
	}
	else if(!last_alloc_half && (alignment == cache_size / 2)) {
		last_alloc_half = true;
	}

	// get alignment size
	unsigned aligned_size = align(ssize, alignment);

	// reallocate if not enough space left
	if((char*)buf_cur + aligned_size > (char*)buf_start + buf_size) {
		nrealloc();
	}

	// service allocation request
	buf_old = buf_cur;
	buf_cur = (char*)buf_cur + aligned_size;
	return buf_old;
}

/* nfree() - "frees" space (in practice this does nothing unless the allocation was the last request) */
void numa_allocator::nfree(void *ptr, unsigned ssize) {
	// get alignment size
	int alignment = (ssize <= cache_size / 2)? cache_size / 2: cache_size;
	unsigned aligned_size = align(ssize, alignment);

	// only "free" if last allocation
	if(!memcmp(ptr, buf_old, aligned_size)) {
		buf_cur = buf_old;
		memset(buf_cur, 0, aligned_size);
		if(last_alloc_half && (alignment == cache_size / 2)) {
			last_alloc_half = false;
		}
	}
}

/* nreset() - frees all memory buffers */
void numa_allocator::nreset(void) {
	if(!empty) {
		empty = true;
		// free other_buffers, if used
		if(other_buffers != NULL) {
			int i = num_buffers - 1;
			while(i >= 0) {
				numa_free(other_buffers[i], buf_size);
				i--;
			}
			free(other_buffers);
		}
		// free primary buffer
		numa_free(buf_start, buf_size);
	}
}

/* nrealloc() - allocates a new buffer */
void numa_allocator::nrealloc(void) {
	// increase size of our old_buffers to store the previously allocated memory
	num_buffers++;
	if(other_buffers == NULL) {
		assert(num_buffers == 1);
		other_buffers = (void**)malloc(num_buffers * sizeof(void*));
		*other_buffers = buf_start;
	} else {
		void** new_bufs = (void**)malloc(num_buffers * sizeof(void*));
		for(int i = 0; i < num_buffers - 1; ++i) {
			new_bufs[i] = other_buffers[i];
		}
		new_bufs[num_buffers-1] = buf_start;
		free(other_buffers);
		other_buffers = new_bufs;
	}

	// allocate new buffer & update pointers and total size
	buf_cur = buf_start = numa_alloc_local(buf_size);
}

/* align() - gets the aligned size given requested size */
inline unsigned numa_allocator::align(unsigned old, unsigned alignment) {
	return old + ((alignment - (old % alignment))) % alignment;
}
