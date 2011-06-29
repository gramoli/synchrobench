#include "queue.h"

val_t rhint;
val_t lhint;

node_t *new_node(val_t val, node_t *next, int transactional)
{
	node_t *node;
	
	if (!transactional) {
		node = (node_t *)malloc(sizeof(node_t));
	} else {
		node = (node_t *)MALLOC(sizeof(node_t));
	}
	if (node == NULL) {
		perror("malloc");
		exit(1);
	}
	
	return node;
}

circarr_t *queue_new()
{
	circarr_t *queue = A; 
	int i, middle = MAX/2;
	
	for (i = 0; i <= MAX+1; i++) {
		queue[i] = (node_t *)malloc(sizeof(node_t *));
	}
	for (i = 0; i < middle; i++)
		queue[i]->val = LN;
	for (i = middle; i <= MAX+1; i++) 
		queue[i]->val = RN;
	lhint = middle-1;
	rhint = middle;
	return queue;
}

void queue_delete(circarr_t *queue)
{
  //free(queue);
}

int queue_size(circarr_t *queue)
{
	int i, size=0;
	for (i=1; i<=MAX+1; i++) {
		if (queue[i]->val != LN && queue[i]->val != RN) size++;
	}
		
	return size;
}


