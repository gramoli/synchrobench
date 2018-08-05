/**
 * queue.cpp - SPSC queue for NUMA zones
 * 	Based on Dmitry Vyukov's C implementation
 *
 * Author: Henry Daly, 2018
 *
 */

#include <numa.h>
#include <stdio.h>
#include "queue.h"
#include "common.h"

/* new_qnode() - allocates an empty qnode */
q_node* new_qnode(node_t* ptr) {
	q_node* new_job = (q_node*)malloc(sizeof(q_node));
	new_job->node = ptr;
	new_job->next = 0;
	return new_job;
}
/* Constructor */
update_queue::update_queue()
{
	stub = new_qnode(0);
	head = tail = stub;
}

/* Destructor */
update_queue::~update_queue()
{
	q_node* f = head;
	while(f) {
		q_node* x = f->next;
		free(f);
		f = x;
	}
}

/* push() - pushes node to queue */
void update_queue::push(node_t* ptr) {
	q_node* spot = new_qnode(ptr);
	tail->next = spot;
	tail = spot;

}

/* pop() - attempts to pop node from queue */
q_node* update_queue::pop(void) {
	q_node* popped = head;
	if(!head->next) return NULL;
	head = head->next;
	popped->node = head->node;
	return popped;
}

