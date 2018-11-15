/*
 * Interface for search layer object
 *
 * Author: Henry Daly, 2017
 */
#ifndef SEARCH_H_
#define SEARCH_H_

#include "queue.h"
#include "skiplist.h"

// Uncomment to collect background stats - reduces performance
//#define BG_STATS

#ifdef BG_STATS
	typedef struct bg_stats bg_stats_t;
	struct bg_stats {
			int raises;
			int loops;
			int lowers;
			int delete_succeeds;
	};
#endif

class search_layer {
private:
	inode_t* 		sentinel;
	pthread_t 		helper;
	int 			numa_zone;
	update_queue*	updates;
public:
	bool finished;
	bool running;
	int bg_non_deleted;
	int bg_tall_deleted;
	int sleep_time;
	bool repopulate;

	search_layer(int nzone, inode_t* ssentinel, update_queue* q);;
	~search_layer();
	void start_helper(int);
	void stop_helper(void);
	inode_t* get_sentinel(void);
	inode_t* set_sentinel(inode_t*);
	int get_zone(void);
	update_queue* get_queue(void);
	void reset_sentinel(void);

#ifdef ADDRESS_CHECKING
	bool			index_ignore;
	volatile long 	bg_local_accesses;
	volatile long 	bg_foreign_accesses;
	volatile long 	ap_local_accesses;
	volatile long 	ap_foreign_accesses;
#endif
	#ifdef BG_STATS
	bg_stats_t shadow_stats;
	void bg_stats(void);
#endif

};

#endif
