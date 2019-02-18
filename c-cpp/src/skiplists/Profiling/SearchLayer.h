#ifndef SEARCH_LAYER_H
#define SEARCH_LAYER_H

#include "Nodes.h"
#include "JobQueue.h"
#include <pthread.h>

typedef struct searchLayer {
	inode_t* sentinel;
	pthread_t helper;
	int numaZone;
	job_queue_t* updates;
	volatile char finished;
	volatile char running;
	int sleep_time;
	int index_ignore;
	volatile long bg_local_accesses;
	volatile long bg_foreign_accesses;
	volatile long ap_local_accesses;
	volatile long ap_foreign_accesses;
} searchLayer_t;

//driver functions
void* updateNumaZone(void* args);
int runJob(inode_t* sentinel, q_node_t* job, int zone, searchLayer_t* numask);

//helper functions
searchLayer_t* constructSearchLayer(inode_t* sentinel, int zone);
searchLayer_t* destructSearchLayer(searchLayer_t* searcher);
int searchLayerSize(searchLayer_t* numask);
void start(searchLayer_t* numask, int sleep_time);
void stop(searchLayer_t* numask);

#endif