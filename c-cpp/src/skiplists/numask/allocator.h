/*
 * Interface for custom allocator
 *
 *  Author: Henry Daly, 2018
 */
#ifndef NUMA_ALLOCATOR_H_
#define NUMA_ALLOCATOR_H_

#include <stdlib.h>

class numa_allocator {
private:
	void*		buf_start;
	unsigned	buf_size;
	void*		buf_cur;
	bool		empty;
	void*		buf_old;
	unsigned	cache_size;
	// for keeping track of the number of buffers
	void**		other_buffers;
	unsigned	num_buffers;
	// for half cache line alignment
	bool		last_alloc_half;

	void nrealloc(void);
	void nreset(void);
	inline unsigned align(unsigned old, unsigned alignment);

public:
	numa_allocator(unsigned ssize);
	~numa_allocator();
	void* nalloc(unsigned size);
	void nfree(void *ptr, unsigned size);
};

#endif /* ALLOCATOR_H_ */
