#ifndef SEARCH_LAYER_C
#define SEARCH_LAYER_C

#define _GNU_SOURCE
#include "SearchLayer.h"
#include "SkipListLazyLock.h"
#include "JobQueue.h"
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <numaif.h>
#include <atomic_ops.h>
#include <numa.h>
#include <sched.h>

searchLayer_t* constructSearchLayer(inode_t* sentinel, int zone) {
	searchLayer_t* numask = (searchLayer_t*)malloc(sizeof(searchLayer_t));
	numask -> sentinel = sentinel;
	numask -> numaZone = zone;
	numask -> updates = constructJobQueue();
	numask -> finished = 0;
	numask -> running = 0;
	numask -> sleep_time = 0;
	return numask;
}

searchLayer_t* destructSearchLayer(searchLayer_t* numask) {
	stop(numask);
	destructJobQueue(numask -> updates);
	free(numask);
}

int searchLayerSize(searchLayer_t* numask) {
	inode_t* runner = numask -> sentinel;
	int size = -2;
	while (runner != NULL) {
		size++;
		runner = runner -> next[0];
	}
	return size;
}

void start(searchLayer_t* numask, int sleep_time) {
	numask -> sleep_time = sleep_time;
	if (numask -> running == 0) {
		numask -> running = 1;
		numask -> finished = 0;
		pthread_create(&numask -> helper, NULL, updateNumaZone, (void*)numask);
	}
}

void stop(searchLayer_t* numask) {
	if (numask -> running) {
		numask -> finished = 1;
		pthread_join(numask -> helper, NULL);
		numask -> running = 0;
	}
}

void* updateNumaZone(void* args) {
	searchLayer_t* numask = (searchLayer_t*)args;
	job_queue_t* updates = numask -> updates;
	inode_t* sentinel = numask -> sentinel;
	const int numaZone = numask -> numaZone;

	//Pin to Zone & CPU
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(numaZone, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

	while (numask -> finished == 0) {
		usleep(numask -> sleep_time);
		while (numask -> finished == 0 && runJob(sentinel, pop(updates), numask -> numaZone)) {}
	}

	return NULL;
}

int runJob(inode_t* sentinel, q_node_t* job, int zone) {
	if (job == NULL) {
		return 0;
	}
	else if (job -> operation == INSERTION) {
		add(sentinel, job -> val, job -> node, zone);
	}
	else if (job -> operation == REMOVAL) {
		removeNode(sentinel, job -> val, zone);
	}
	return 1;
}

#endif