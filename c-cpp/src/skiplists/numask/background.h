/*
 * Interface for the background thread functions.
 *
 * Author: Henry Daly, 2017
 */
#ifndef BACKGROUND_H_
#define BACKGROUND_H_

#include "skiplist.h"

// arguments for data-layer-helper thread
struct bg_dl_args {
	node_t*		head;
	int			tsleep;
	bool*		done;
};

/* Public background thread interface */
void bg_remove(node_t* prev, node_t* node);
void* per_NUMA_helper(void* args);
void* data_layer_helper(void* data);

#endif /* BACKGROUND_H_ */
