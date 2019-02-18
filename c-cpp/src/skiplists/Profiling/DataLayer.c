#ifndef DATA_LAYER_C
#define DATA_LAYER_C

#include "DataLayer.h"
#include "Atomic.h"
#include <pthread.h>
#include <assert.h>
#include <unistd.h>

dataLayerThread_t *remover = NULL;

//Helper Functions
inline node_t* getElement(inode_t* sentinel, const int val);
inline void dispatchSignal(int val, node_t* dataLayer, Job operation);
inline int validateLink(node_t* previous, node_t* current);

inline node_t* getElement(inode_t* sentinel, const int val) {
	inode_t *previous = sentinel, *current = NULL;
	for (int i = previous -> topLevel - 1; i >= 0; i--) {
		current = previous -> next[i];
		while (current -> val < val) {
			previous = current;
			current = current -> next[i];
		}
	}
	return previous -> dataLayer;
}

inline void dispatchSignal(int val, node_t* dataLayer, Job operation) {
	assert(numaLayers != NULL);
	for (int i = 0; i < numberNumaZones; i++) {
		push(numaLayers[i] -> updates, val, operation, dataLayer);
	}
}

inline int validateLink(node_t* previous, node_t* current) {
	return previous -> next == current;
}

int lazyFind(searchLayer_t* numask, int val) {
	node_t* current = getElement(numask -> sentinel, val);
	while (current -> val < val) {
		current = current -> next;
	}
	return current -> val == val && current -> markedToDelete == 0;
}

int lazyAdd(searchLayer_t* numask, int val) {
	char retry = 1;
	while (retry) {
		node_t* previous = getElement(numask -> sentinel, val);
		node_t* current = previous -> next;
		while (current -> val < val) {
			previous = current;
			current = current -> next;
		}
		pthread_mutex_lock(&previous -> lock);
		pthread_mutex_lock(&current -> lock);
		if (validateLink(previous, current)) {
			if (current -> val == val) { //incorrect! need to check if markedToDelete, then we good
				pthread_mutex_unlock(&previous -> lock);
				pthread_mutex_unlock(&current -> lock);
				return 0;
			}
			node_t* insertion = constructNode(val, numberNumaZones); //automatically set as fresh
			insertion -> next = current;
			previous -> next = insertion;
			pthread_mutex_unlock(&previous -> lock);
			pthread_mutex_unlock(&current -> lock);
			return 1;
		}
		pthread_mutex_unlock(&previous -> lock);
		pthread_mutex_unlock(&current -> lock);
	}
}

int lazyRemove(searchLayer_t* numask, int val) {
	node_t* previous = getElement(numask -> sentinel, val);
	node_t* current = previous -> next;
	while (current -> val < val) {
		previous = current;
		current = current -> next;
	}

	if (current -> val != val || current -> markedToDelete == 1) {
		return 0;
	}
	//incorporate atomicity here with CAS
	if (__sync_val_compare_and_swap(&current -> markedToDelete, 0, 1) == 0) {
		current -> fresh = 1;
		return 1;
	}
	return 0;
}

void* backgroundRemoval(void* input) {
	dataLayerThread_t* thread = (dataLayerThread_t*)input;
	node_t* sentinel = thread -> sentinel;
	while (thread -> finished == 0) {
		usleep(thread -> sleep_time);
		node_t* previous = sentinel;
		node_t* current = sentinel -> next;
		while (current -> next != NULL) {
			if (current -> fresh) {
				current -> fresh = 0; //unset as fresh, need a CAS here? only thread operating on structure
				if (current -> markedToDelete) {
					dispatchSignal(current -> val, current, REMOVAL);
				}
				else {
					dispatchSignal(current -> val, current, INSERTION);
				}
			}
			else if (current -> markedToDelete && current -> references == 0) {
				int valid = 0;
				pthread_mutex_lock(&previous -> lock);
				pthread_mutex_lock(&current -> lock);
				if ((valid = validateLink(previous, current)) != 0) {
					previous -> next = current -> next;
				}
				pthread_mutex_unlock(&previous -> lock);
				pthread_mutex_unlock(&current -> lock);
				if (valid) {
					current = current -> next;
					continue;
				}
			}
			previous = current;
			current = current -> next;
		}
	}
	return NULL;
}

static dataLayerThread_t* constructDataLayerThread() {
	dataLayerThread_t* thread = (dataLayerThread_t*)malloc(sizeof(dataLayerThread_t));
	thread -> running = 0;
	thread -> finished = 0;
	thread -> sleep_time = 10000;
	thread -> sentinel = NULL;
	return thread;
}

void startDataLayerThread(node_t* sentinel) {
	if (remover == NULL) {
		remover = constructDataLayerThread();
	}
	if (remover -> running == 0) {
		remover -> running = 1;
		remover -> finished = 0;
		remover -> sentinel = sentinel;
		pthread_create(&remover -> runner, NULL, backgroundRemoval, (void*)remover);
	}
}

void stopDataLayerThread() {
	if (remover -> running) {
		remover -> finished = 1;
		//CAS(&remover -> finished, remover -> finished, 1); //add a while loop around this to ensure it happens
		pthread_join(remover -> runner, NULL);
		remover -> running = 0;
	}
}

#endif