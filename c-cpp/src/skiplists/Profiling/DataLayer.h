#ifndef DATA_LAYER_H
#define DATA_LAYER_H

#include "SearchLayer.h"
#include "Nodes.h"
#include "JobQueue.h"

extern searchLayer_t** numaLayers; 
extern int numberNumaZones;
typedef struct dataLayerThread_t {
	pthread_t runner;
	volatile char running;
	volatile char finished;
	int sleep_time;
	node_t* sentinel;
} dataLayerThread_t;


//Driver Functions
int lazyFind(searchLayer_t* numask, int val);
int lazyAdd(searchLayer_t* numask, int val);
int lazyRemove(searchLayer_t* numask, int val);

//Background functions
void* backgroundRemoval(void* input);
void startDataLayerThread(node_t* sentinel);
void stopDataLayerThread();

#endif