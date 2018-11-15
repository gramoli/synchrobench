/*
 * search.cpp: search layer definition
 *
 *      Author: Henry Daly, 2017
 */


/**
 * Module Overview:
 *
 * The search_layer class provides an abstraction for the index and intermediate
 * layer of a NUMA zone. One object is created per NUMA zone.
 *
 */

#include <pthread.h>
#include "search.h"
#include "queue.h"
#include "skiplist.h"
#include "background.h"
#include "stdio.h"

//local numa search layer
typedef struct SkipList {
  struct node* head; //stores the head sentinel node
  int maxLevel; //stores the max number of levels, declared at runtime by user
} skipList_t;

skipList_t* constructSkipList(int maxLevel);
size_t getSize(skipList_t* skipList);


//constructor for skip list that initializes sentinel nodes
skipList_t* constructSkipList(int maxLevel) {
	skipList_t* skipList = (skipList_t*)malloc(sizeof(skipList_t));
  	node_t* tail = constructLinkedNode(INT_MAX, maxLevel, NULL);
  	node_t* head = constructLinkedNode(INT_MIN, maxLevel, tail);
  	tail -> fullylinked = 1;
  	head -> fullylinked = 1;
  	skipList -> head = head;
  	skipList -> maxLevel = maxLevel;
  	return skipList;
}

//destructor for skip list that frees all data and locks
skipList_t* destructSkipList(skipList_t* skipList) {
	node_t *runner = skipList -> head, *temp = NULL;
  	while (runner != NULL) {
    	temp = runner -> next[0];
    	destructNode(runner);
    	runner = temp;
  	}
  	free(skipList);
  	return NULL;
}

//gets size of skip list, not concurrent
size_t getSize(skipList_t* skipList) {
  	int size = -1;
	node_t* runner = skipList -> head -> next[0];
  	while (runner -> next[0] != NULL) {
    	if (runner -> fullylinked && runner -> markedToDelete == 0) {
      		size++;
    	}
    	runner = runner -> next[0];
  	}
  	return size;
}



/* Constructor */
search_layer::search_layer(int nzone, inode_t* ssentinel, update_queue* q)
:finished(false), running(false), numa_zone(nzone), bg_tall_deleted(0), bg_non_deleted(0),
 repopulate(false), sentinel(ssentinel), updates(q), sleep_time(0), helper(0)
{
	srand(time(NULL));
}

/* Destructor */
search_layer::~search_layer()
{
	if(!finished) {
		stop_helper();
	}
}

/* start_helper() - starts helper thread */
void search_layer::start_helper(int ssleep_time) {
	sleep_time = ssleep_time;
	if(!running) {
			running = true;
			finished = false;
			pthread_create(&helper, NULL, per_NUMA_helper, (void*)this);
	}
}
/* stop_helper() - stops helper thread */
void search_layer::stop_helper(void) {
	if(running) {
			finished = true;
			pthread_join(helper, NULL);
			//BARRIER();
			running = false;
	}
}

/* get_sentinel() - return sentinel index node of search layer */
inode_t* search_layer::get_sentinel(void) {
	return sentinel;
}

/* set_sentinel() - update and return new sentinel node */
inode_t* search_layer::set_sentinel(inode_t* new_sent) {
	return (sentinel = new_sent);
}

/* get_node() - return NUMA zone of search layer */
int search_layer::get_zone(void) {
	return numa_zone;
}

/* get_queue() - return queue of search layer */
update_queue* search_layer::get_queue(void) {
	return updates;
}

/* reset_sentinel() - sets flag to later reset sentinel to fix towers */
void search_layer::reset_sentinel(void) {
	repopulate = true;
}