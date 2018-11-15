#ifndef QUEUE_H_
#define QUEUE_H_

#include "skiplist-lock.h"

typedef struct q_node {
	node_t* job;
	struct q_node* next;
} q_node_t;

q_node_t* constructQNode(node_t* job) {
	q_node_t* new_job = (q_node_t*)malloc(sizeof(q_node_t));
	new_job -> job = job;
	new_job -> next = NULL;
	return new_job;
}

typedef struct job_queue {
	q_node_t* head;
	q_node_t* tail;
	q_node_t* sentinel;
} job_queue_t;

job_queue_t constructJobQueue() {
	job_queue_t jobs;
	jobs.sentinel = constructQNode(NULL);
	jobs.tail = jobs.head = jobs.sentinel;
	return jobs;
}

void destructJobQueue(job_queue_t jobs) {
	q_node_t* runner = jobs.head;
	while (runner != NULL) {
		q_node_t* temp = runner;
		runner = runner -> next;
		free(runner);
	}
}

void push(job_queue_t jobs, node_t* job) {
	q_node_t* new_job = constructQNode(job);
	jobs.tail -> next = new_job;
	jobs.tail = new_job;
}

q_node_t* pop(job_queue_t jobs) {
	if (jobs.head -> next == NULL) {
		return NULL;
	}
	q_node_t* front = jobs.head;
	jobs.head = jobs.head -> next;
	front -> node = jobs.head -> node;
	return front;
}

#endif
