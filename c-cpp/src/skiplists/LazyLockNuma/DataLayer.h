#ifndef DATA_LAYER_H
#define DATA_LAYER_H

#include "SearchLayer.h"
#include "Nodes.h"
#include "JobQueue.h"

//Driver Functions
int lazyFind(search_layer_t* numask, int val);
int lazyAdd(searchLayer_t* numask, int val);
int lazyRemove(searchLayer_t* numask, int val);

//Helper Functions
inline node_t* getPreviousElement(inode_t* sentinel, const int val);
inline void dispatchSignal(int val, Job operation);
inline int validateLink(node_t* previous, node_t* current);

#endif