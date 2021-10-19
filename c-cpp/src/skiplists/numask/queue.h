/*
 * Interface for SPSC Queue
 *
 * Author: Henry Daly, 2018
 */

#ifndef QUEUE_H_
#define QUEUE_H_

#include "skiplist.h"
#include "common.h"

struct q_node {
	q_node*	next;
	node_t*	node;
};

class update_queue{
private:
	q_node* head;
	q_node* tail;
	q_node* stub;

public:
			update_queue();
			~update_queue();
	void	push(node_t* ptr);
	q_node*	pop();
};

#endif /* QUEUE_H_ */
