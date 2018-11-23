#ifndef DATA_LAYER_H
#define DATA_LAYER_H

#include "SearchLayer.h"
#include "Nodes.h"
#include "JobQueue.h"

extern searchLayer_t** numaLayers; 
extern int numberNumaZones;
struct DataLayerThread {
	pthread_t runner;
	volatile char running;
	volatile char finished;
	int sleep_time;
} remover;


//Driver Functions
int lazyFind(searchLayer_t* numask, int val);
int lazyAdd(searchLayer_t* numask, int val);
int lazyRemove(searchLayer_t* numask, int val);

//Background functions
void* backgroundRemoval(void* input);
void startDataLayerThread(node_t* sentinel);
void stopDataLayerThread();

#endif