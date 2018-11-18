#ifndef JOB_QUEUE_C
#define JOB_QUEUE_C

#include "JobQueue.h"

q_node_t* constructQNode(int val, Job operation) {
	q_node_t* new_job = (q_node_t*)malloc(sizeof(q_node_t));
	new_job -> val = val;
	new_job -> operation = operation;
	return new_job;
}

job_queue_t* constructJobQueue() {
	job_queue_t* jobs = (job_queue_t*)malloc(sizeof(job_queue_t));
	jobs -> sentinel = constructQNode(0, NONE);
	jobs -> tail = jobs -> head = jobs -> sentinel;
	return jobs;
}

void destructJobQueue(job_queue_t* jobs) {
	q_node_t* runner = jobs -> head;
	while (runner != NULL) {
		q_node_t* temp = runner;
		runner = runner -> next;
		free(runner);
	}
}

void push(job_queue_t* jobs, int val, Job operation) {
	q_node_t* new_job = constructQNode(val, operation);
	jobs -> tail -> next = new_job;
	jobs -> tail = new_job;
}

q_node_t* pop(job_queue_t* jobs) {
	if (jobs -> head -> next == NULL) {
		return NULL;
	}
	q_node_t* front = jobs -> head;
	jobs -> head = jobs -> head -> next;
	front -> val = jobs -> head -> val;
    front -> operation = jobs -> head -> operation;
	return front;
}

#endif