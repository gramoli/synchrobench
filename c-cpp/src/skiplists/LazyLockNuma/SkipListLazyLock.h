#ifndef SKIPLISTLAZYLOCK_H
#define SKIPLISTLAZYLOCK_H

#include "Nodes.h"

//driver functions
int add(inode_t *sentinel, int val, int zone);
int removeNode(inode_t *sentinel, int val, int zone) ;

#endif