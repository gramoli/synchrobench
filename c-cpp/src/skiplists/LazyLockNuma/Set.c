#ifndef SET_H
#define SET_H

#include "Set.h"

int sl_contains(searchLayer_t* numask, int val) {
	return lazyFind(numask, val);
}

int sl_add(searchLayer_t* numask, int val) {
	return lazyAdd(numask, val);
}

int sl_remove(searchLayer_t* numask, int val) {
	return lazyRemove(numask, val);
}

int sl_size(node_t* sentinel) {
	int size = -1;
	node_t* runner = sentinel;
	while (runner -> next != NULL) {
		runner = runner -> next;
		size++;
	}
	return size;
}

#endif