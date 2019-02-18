#ifndef SKIPLISTLAZYLOCK_H
#define SKIPLISTLAZYLOCK_H

#include "Nodes.h"

//driver functions
int add(inode_t *sentinel, int val, node_t* dataLayer, int zone, volatile int* bg_local_accesses, volatile int* bg_foreign_accesses, int index_ignore);
int removeNode(inode_t *sentinel, int val, int zone, volatile int* bg_local_accesses, volatile int* bg_foreign_accesses, int index_ignore) ;

#endif