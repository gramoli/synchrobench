#ifndef DATA_LAYER_C
#define DATA_LAYER_C

#include "DataLayer.h"
#include "LinkedListLazyLock.h"
#include <pthread.h>

extern search_layer** numaLayers; 
extern int numberNumaZones;

inline node_t* getPreviousElement(inode_t* sentinel, const int val) {
	inode_t* previous = sentinel, current = NULL;
	for (int i = previous -> topLevel - 1; i >= 0; i--) {
		current = previous -> next[i];
		while (current -> val < val) {
			previous = current;
			current = current -> next[i];
		}
	}
	return previous -> dataLayer;
}

inline void dispatchSignal(int val, Job operation) {
	assert(numaLayers != NULL);
	for (int i = 0; i < numberNumaZones; i++) {
		push(numaLayers[i] -> updates, val, operation);
	}
}

inline int validateLink(node_t* previous, node_t* current) {
	return (previous -> markedToDelete == 0 && current -> markedToDelete == 0) && previous -> next == current;
}

int lazyFind(search_layer_t* numask, int val) {
	node_t* current = getPreviousElement(numask -> sentinel, val);
	while (current -> val < val) {
		current = current -> next;
	}
	return current -> val == val && current -> markedToDelete == 0;
}

int lazyAdd(searchLayer_t* numask, int val) {
	char retry = 1;
	while (retry) {
		node_t* previous = getPreviousElement(numask -> sentinel, val);
		node_t* current = previous -> next;
		while (current -> val < val) {
			previous = current;
			current = current -> next;
		}
		pthread_mutex_lock(&previous -> lock);
		pthread_mutex_lock(&current -> lock);
		if (validateLink(previous, current)) {
			if (current -> val == val) {
				pthread_mutex_unlock(&previous -> lock);
				pthread_mutex_unlock(&current -> lock);
				return 0;
			}
			node_t* insertion = constructNode(val);
			insertion -> next = current;
			previous -> next = insertion;
			pthread_mutex_unlock(&previous -> lock);
			pthread_mutex_unlock(&current -> lock);
			dispatchSignal(current -> val, REMOVAL);
			return 1;
		}
		pthread_mutex_unlock(&previous -> lock);
		pthread_mutex_unlock(&current -> lock);
	}
}


int lazyRemove(searchLayer_t* numask, int val) {
	char retry = 1;
	while (retry) {
		node_t* previous = getPreviousElement(numask -> sentinel, val);
		node_t* current = previous -> next;
		while (current -> val < val) {
			previous = current;
			current = current -> next;ß
		}
		pthread_mutex_lock(&previous -> lock);
		pthread_mutex_lock(&current -> lock);
		if (validateLink(previous, current)) {
			if (current -> val != val) {
				pthread_mutex_unlock(&previous -> lock);
				pthread_mutex_unlock(&current -> lock);
				return 0;
			}
			current -> markedToDelete = 1;
			previous -> next = current -> next;
			pthread_mutex_unlock(&previous -> lock);
			pthread_mutex_unlock(&current -> lock);
			dispatchSignal(current -> val, REMOVAL);
			return 1;
		}
		pthread_mutex_unlock(&previous -> lock);
		pthread_mutex_unlock(&current -> lock);
	}
}

#endif