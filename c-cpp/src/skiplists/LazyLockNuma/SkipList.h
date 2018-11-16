#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <atomic_ops.h>

#define INT_MIN -2147483648
#define INT_MAX 2147483647

numa_allocator_t** allocators;
int levelmax;

//data layer node
typedef struct node {
 	int val; //stores the value at the node
  	struct node* next; //stores the next data layer node
  	volatile int markedToDelete; //stores whether marked for deletion
  	volatile int references; //stores the number of pointers pointing to the data layer node form the numa zones
  	pthread_mutex_t lock; //node-specific mutex
} node_t;

node_t* constructNode(int val);
node_t* constructLinkedNode(int val, node_t* next);

//index layer node
typedef struct inode {
	int val; //stores the value at the node
	int topLevel; //stores the height of the tower
	struct inode** next; //stores the next pointer for each level in the tower
	struct node* dataLayer; //stores a pointer to the tower's pernuma link into the data layer
	volatile int markedToDelete; //stores whether marked for deletion
	volatile int fullylinked; //stores whether fullylinked or in the middle of insertion
	pthread_mutex_t lock; //node-specific mutex
} inode_t;

inode_t* constructIndexNode(int val, int topLevel, int zone);
inode_t* constructLinkedIndexNode(int val, int topLevel, int zone, inode_t* next);

//base skip list
typedef struct skipList {
	struct node* head;
} skipList_t;

skipList_t* constructSkipList();
size_t getSize(skipList_t* set);

//Helper Methods
int floor_log_2(unsigned int n);
int getRandomLevel(unsigned int maxLevel);

#endif