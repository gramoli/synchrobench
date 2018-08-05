/*
 * search.cpp: search layer definition
 *
 *      Author: Henry Daly, 2017
 */


/**
 * Module Overview:
 *
 * The search_layer class provides an abstraction for the index and intermediate
 * layer of a NUMA zone. One object is created per NUMA zone.
 *
 */

#include <pthread.h>
#include "search.h"
#include "queue.h"
#include "skiplist.h"
#include "background.h"
#include "stdio.h"


/* Constructor */
search_layer::search_layer(int nzone, inode_t* ssentinel, update_queue* q)
:finished(false), running(false), numa_zone(nzone), bg_tall_deleted(0), bg_non_deleted(0),
 repopulate(false), sentinel(ssentinel), updates(q), sleep_time(0), helper(0)
{
	srand(time(NULL));
#ifdef BG_STATS
	shadow_stats.loops = 0;
	shadow_stats.raises = 0;
	shadow_stats.lowers = 0;
	shadow_stats.delete_succeeds = 0;
#endif
#ifdef ADDRESS_CHECKING
	bg_local_accesses = 0;
	bg_foreign_accesses = 0;
	ap_local_accesses = 0;
	ap_foreign_accesses = 0;
	index_ignore = true;
#endif
}

/* Destructor */
search_layer::~search_layer()
{
	if(!finished) {
		stop_helper();
	}
}

/* start_helper() - starts helper thread */
void search_layer::start_helper(int ssleep_time) {
	sleep_time = ssleep_time;
	if(!running) {
			running = true;
			finished = false;
			pthread_create(&helper, NULL, per_NUMA_helper, (void*)this);
	}
}
/* stop_helper() - stops helper thread */
void search_layer::stop_helper(void) {
	if(running) {
			finished = true;
			pthread_join(helper, NULL);
			//BARRIER();
			running = false;
	}
}

/* get_sentinel() - return sentinel index node of search layer */
inode_t* search_layer::get_sentinel(void) {
	return sentinel;
}

/* set_sentinel() - update and return new sentinel node */
inode_t* search_layer::set_sentinel(inode_t* new_sent) {
	return (sentinel = new_sent);
}

/* get_node() - return NUMA zone of search layer */
int search_layer::get_zone(void) {
	return numa_zone;
}

/* get_queue() - return queue of search layer */
update_queue* search_layer::get_queue(void) {
	return updates;
}

/* reset_sentinel() - sets flag to later reset sentinel to fix towers */
void search_layer::reset_sentinel(void) {
	repopulate = true;
}

#ifdef BG_STATS
/**
 * bg_stats() - print background statistics
 */
void search_layer::bg_stats(void)
{
	printf("Loops = %d\n", shadow_stats.loops);
	printf("Raises = %d\n", shadow_stats.raises);
	printf("Lowers = %d\n", shadow_stats.lowers);
	printf("Delete Succeeds = %d\n", shadow_stats.delete_succeeds);
}
#endif

