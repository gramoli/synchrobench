#include "skiplist-lock.h"

/*
 * Returns a random level for inserting a new node, results are hardwired to p=0.5, min=1, max=32.
 *
 * "Xorshift generators are extremely fast non-cryptographically-secure random number generators on
 * modern architectures."
 *
 * Marsaglia, George, (July 2003), "Xorshift RNGs", Journal of Statistical Software 8 (14)
 */
int getRandomLevel(unsigned int maxLevel) {
	static uint32_t y = 2463534242UL;
	y ^= y << 13;
	y ^= y >> 17;
	y ^= y << 5;

	uint32_t temp = y;
	uint32_t level = 1;
	while (((temp >>= 1) & 1) != 0) {
		level++;
	}

	if (level > maxLevel) {
		return (int)maxLevel;
	}
	return (int)level;
}

int floor_log_2(unsigned int n) {
	int pos = 0;
	if (n >= 1 << 16) { n >>= 16; pos += 16; }
	if (n >= 1 << 8)  { n >>=  8; pos +=  8; }
	if (n >= 1 << 4)  { n >>=  4; pos +=  4; }
	if (n >= 1 << 2)  { n >>=  2; pos +=  2; }
	if (n >= 1 << 1)  {           pos +=  1; }
	return ((n == 0) ? (-1) : pos);
}

//constructor that initializes data fields of the node and its lock,
//but does not link the node to other towers
node_t* constructNode(int val, int topLevel) {
	node_t* node = (node_t*)malloc(sizeof(node_t));
	node -> next = (node_t**)malloc(topLevel * sizeof(node_t*));
	node -> val = val;
	node -> topLevel = topLevel;
	node -> markedToDelete = 0;
	node -> fullylinked = 0;
	pthread_mutex_init(&node -> lock, NULL);
	return node;
}

//constructor that initializes data fields of the node and its lock, as well as
//pointing each level of the tower to the provided next node
node_t* constructLinkedNode(int val, int topLevel, node_t* next) {
	node_t* node = constructNode(val, topLevel);
	for (int i = 0; i < topLevel; i++) {
		node -> next[i] = next;
	}
	return node;
}

void destructNode(node_t* node) {
 	pthread_mutex_destroy(&node -> lock);
 	free(node -> next);
 	free(node);
}

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
