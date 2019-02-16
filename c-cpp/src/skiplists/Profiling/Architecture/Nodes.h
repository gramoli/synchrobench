#ifndef NODES_H
#define NODES_H

#include <stdlib.h>
#include <pthread.h>
#include "Allocator.h"

#define INT_MIN -2147483648
#define INT_MAX 2147483647

extern numa_allocator_t** allocators;
extern unsigned int levelmax;

//data layer node
typedef struct node {
 	int val; //stores the value at the node
  	volatile int markedToDelete; //stores whether marked for deletion
  	volatile int references; //stores the number of pointers pointing to the data layer node form the numa zones
  	volatile int fresh; //identifies whether the node was recently removed/inserted, and needs to be propogated to index layers
  	struct node* next; //stores the next data layer node
  	pthread_mutex_t lock; //node-specific mutex
} node_t;

node_t* constructNode(int val, int intitialReferences);
node_t* constructLinkedNode(int val, int intitialReferences, node_t* next);

//index layer node
typedef struct inode {
	int val; //stores the value at the node
	int topLevel; //stores the height of the tower
	struct inode** next; //stores the next pointer for each level in the tower
	struct node* dataLayer; //stores a pointer to the tower's pernuma link into the data layer
} inode_t;

inode_t* constructIndexNode(int val, int topLevel, node_t* dataLayer, int zone);
inode_t* constructLinkedIndexNode(int val, int topLevel, node_t* dataLayer, int zone, inode_t* next);

//Helper Methods
int floor_log_2(unsigned int n);
int getRandomLevel(unsigned int maxLevel);

#endif