#ifndef SET_H
#define SET_H

#include "DataLayer.h"
#include "SearchLayer.h"
#include "Allocator.h"

searchLayer_t** numaLayers;
numa_allocator_t** allocators;
int numberNumaZones;
unsigned int levelmax;

int sl_contains(searchLayer_t* numask, int val);
int sl_add(searchLayer_t* numask, int val);
int sl_remove(searchLayer_t* numask, int val);
int sl_size(node_t* sentinel);

#endif