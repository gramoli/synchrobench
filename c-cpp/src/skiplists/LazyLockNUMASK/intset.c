#include "intset.h"

int sl_contains(skipList_t *set, int val, int transactional) {
	return contains(set, val);
}

int sl_add(skipList_t *set, int val, int transactional) {
	return add(set, val);
}

int sl_remove(skipList_t *set, int val, int transactional) {
	return removeNode(set, val);
}
