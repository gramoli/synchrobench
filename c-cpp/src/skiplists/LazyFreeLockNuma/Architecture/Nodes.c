#ifndef NODES_C
#define NODES_C

#include "Nodes.h"
#include <stdint.h>

//constructor that initializes data fields of the index node and its lock,
//but does not link the node to other towers
inode_t* constructIndexNode(int val, int topLevel, node_t* dataLayer, int zone) {
	numa_allocator_t* allocator = allocators[zone];
	inode_t* node = (inode_t*)(nalloc(allocator, sizeof(inode_t)));
	node -> next = (inode_t**)(nalloc(allocator, topLevel * sizeof(inode_t*)));
	node -> dataLayer = dataLayer;
	node -> val = val;
	node -> topLevel = topLevel;
	return node;
}

//constructor that initializes data fields of the index node and its lock, as well as
//pointing each level of the tower to the provided next node
inode_t* constructLinkedIndexNode(int val, int topLevel,node_t* dataLayer, int zone, inode_t* next) {
	inode_t* node = constructIndexNode(val, topLevel, dataLayer, zone);
	for (int i = 0; i < topLevel; i++) {
		node -> next[i] = next;
	}
	return node;
}

//constructor that initializes the data fields of the data layer node, 
//but does not link it to another node
node_t* constructNode(int val, int initialReferences) {
	node_t* node = (node_t*)malloc(sizeof(node_t));
	node -> val = val;
	node -> next = NULL;
	node -> markedToDelete = 0;
	node -> references = initialReferences; //QUESTION: is this okay? instantiating number of references before actual additions
	node -> fresh = 1; //automatically marked as fresh on construction
	pthread_mutex_init(&node -> lock, NULL);
	return node;
}

//constructor that initializes the data fields of the data layer node node, 
//and also links it to another node
node_t* constructLinkedNode(int val, int initialReferences, node_t* next) {
	node_t* node = constructNode(val, initialReferences);
	node -> next = next;
	return node;
}

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

#endif