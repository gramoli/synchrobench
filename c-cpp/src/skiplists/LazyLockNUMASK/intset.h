#include "lazylock.h"

int sl_contains(skipList_t *set, int val, int transactional);
int sl_add(skipList_t *set, int val, int transactional);
int sl_remove(skipList_t *set, int val, int transactional);
