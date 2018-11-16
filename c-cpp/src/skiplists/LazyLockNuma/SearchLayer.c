#include "SearchLayer.h"
#include "SkipListLazyLock.h"
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <numaif.h>
#include <atomic_ops.h>
#include <numa.h>

searchLayer_t* constructSearchLayer(int zone, inode_t* sentinel, job_queue_t* q) {
	searchLayer_t* numask = (searchLayer_t*)malloc(sizeof(searchLayer_t));
	numask -> finished = false;
	numask -> running = true;
	numask -> numaZone = zone;
	numask -> sentinel = sentinel;
	numask -> updates = q;
	numask -> sleep_time = 0;
}

searchLayer_t* destructSearchLayer(searchLayer_t* numask) {
	if (numask -> finished == 0) {
		stop(numask);
	}
	free(numask);
}

void start(searchLayer_t* numask, int sleep_time) {
	numask -> sleep_time = sleep_time;
	if (numask -> running = 0) {
		numask -> running = 1;
		numask -> finished = 0;
		pthread_create(&numask -> helper, NULL, updateNumaZone, (void*)numask);
	}
}

void stop(searchLayer_t* numask) {
	if (seacher -> running) {
		numask -> finished = 1;
		pthread_join(numask -> helper, NULL);
		numask -> running = 0;
	}
}

void* updateNumaZone(void* args) {
	searchLayer_t* numask = (searchLayer_t*)args;
	job_queue_t* updates = args -> updates;
	inode_t* sentinel = numask -> sentinel;
	const int numaZone = numask -> numaZone;

	//Pin to Zone & CPU
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(numaZone, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

	while (numask -> finished == 0) {
		usleep(numask -> sleep_time);
		while (numask -> finished == 0 && runJob(sentinel, updates -> pop())) {}
	}

	return NULL;
}

int runJob(inode_t* sentinel, q_node* job) {
	if (job == NULL) {
		return 0;
	}
	else if (job -> operation == INSERTION) {
		add(sentinel, job -> value);
	}
	else if (job -> operation == REMOVAL) {
		removeNode(sentinel, job -> value);
	}
	return 1;
}