#ifndef ALLOCATOR_C
#define ALLOCATOR_C

#include <stdlib.h>
#include <numa.h>
#include "Allocator.h"

static void nreset(numa_allocator_t* allocator);
static void nrealloc(numa_allocator_t* allocator);
inline unsigned align(unsigned old, unsigned alignment);

numa_allocator_t* constructAllocator(unsigned ssize) {
	numa_allocator_t* allocator = (numa_allocator_t*)malloc(sizeof(numa_allocator_t));
	allocator -> buf_size = ssize;
	allocator -> empty = 0;
	allocator -> num_buffers = 0;
	allocator -> buf_old = NULL;
	allocator -> other_buffers = NULL;
	allocator -> last_alloc_half = 0;
	allocator -> cache_size = CACHE_LINE_SIZE;
	allocator -> buf_cur = allocator -> buf_old = numa_alloc_local(allocator -> buf_size);
	return allocator;
}

void destructAllocator(numa_allocator_t* allocator, unsigned ssize) {
	nreset(allocator);
	free(allocator);
}

//driver methods
void* nalloc(numa_allocator_t* allocator, unsigned ssize) {
	const unsigned cache_size = allocator -> cache_size;
	char last_alloc_half = allocator -> last_alloc_half;

	//get cache-line alignment for request
	int alignment = (ssize <= cache_size / 2) ? cache_size / 2: cache_size;

	//if the last allocation was half a cache line and we want a full cache line, we move
	//the free space pointer forward a half cache line so we don't spill over cache lines */
	if (last_alloc_half && (alignment == cache_size)) {
		allocator -> buf_cur = (char*)allocator -> buf_cur + (cache_size / 2);
		allocator -> last_alloc_half = 0;
	}
	else if (!last_alloc_half && (alignment == cache_size / 2)) {
		allocator -> last_alloc_half = 1;
	}

	//get alignment size
	unsigned aligned_size = align(ssize, alignment);

	//reallocate if not enough space left
	if ((char*)allocator -> buf_cur + aligned_size > (char*)allocator -> buf_start + allocator -> buf_size) {
		nrealloc(allocator);
	}

	//service allocation request
	allocator -> buf_old = allocator -> buf_cur;
	allocator -> buf_cur = (char*)allocator -> buf_cur + aligned_size;
	return allocator -> buf_old;
}

void nfree(numa_allocator_t* allocator, void *ptr, unsigned ssize) {
	const unsigned cache_size = allocator -> cache_size;

	//get alignment size
	int alignment = (ssize <= cache_size / 2) ? cache_size / 2 : cache_size;
	unsigned aligned_size = align(ssize, alignment);

	//only "free" if last allocation
	if (!memcmp(ptr, allocator -> buf_old, aligned_size)) {
		allocator -> buf_cur = allocator -> buf_old;
		memset(allocator -> buf_cur, 0, aligned_size);
		if (allocator -> last_alloc_half && (alignment == cache_size / 2)) {
			allocator -> last_alloc_half = 0;
		}
	}
}

static void nreset(numa_allocator_t* allocator) {
	if (!allocator -> empty) {
		allocator -> empty = 1;
		//free other_buffers, if used
		if (allocator -> other_buffers != NULL) {
			int i = allocator -> num_buffers - 1;
			while (i >= 0) {
				numa_free(allocator -> other_buffers[i], allocator -> buf_size);
				i--;
			}
			free(allocator -> other_buffers);
		}
		//free primary buffer
		numa_free(allocator -> buf_start, allocator -> buf_size);
	}
}

static void nrealloc(numa_allocator_t* allocator) {
	//increase size of our old_buffers to store the previously allocated memory
	allocator -> num_buffers++;
	if (allocator -> other_buffers == NULL) {
		assert(allocator -> num_buffers == 1);
		allocator -> other_buffers = (void**)malloc(allocator -> num_buffers * sizeof(void*));
		*(allocator -> other_buffers) = allocator -> buf_start;
	} else {
		void** new_bufs = (void**)malloc(allocator -> num_buffers * sizeof(void*));
		for (int i = 0; i < allocator -> num_buffers - 1; i++) {
			new_bufs[i] = allocator -> other_buffers[i];
		}
		new_bufs[allocator -> num_buffers - 1] = allocator -> buf_start;
		free(allocator -> other_buffers);
		allocator -> other_buffers = new_bufs;
	}
	//allocate new buffer & update pointers and total size
	allocator -> buf_cur = allocator -> buf_start = numa_alloc_local(allocator -> buf_size);
}

inline unsigned align(unsigned old, unsigned alignment) {
	return old + ((alignment - (old % alignment))) % alignment;
}

#endif