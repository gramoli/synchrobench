#include "skiplist-lock.h"


//Driver Methods
int contains(skipList_t *set, int val);
int add(skipList_t *set, int val);
int removeNode(skipList_t *set, int val);

//Helper Methods
inline int search(skipList_t *set, int val, node_t **predecessors, node_t **successors);
inline void unlockLevels(node_t **nodes, int topLevel);
inline int validDeletion(node_t *candidate, int idx);
