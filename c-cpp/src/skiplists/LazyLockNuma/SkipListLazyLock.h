#include "SkipList.h"

//driver functions
int contains(inode_t *sentinel, int val);
int add(inode_t *sentinel, int val);
int removeNode(inode_t *sentinel, int val) ;

//helper functions
inline int search(inode_t *sentinel, int val, inode_t **predecessors, inode_t **successors);
inline int validDeletion(inode_t *candidate, int idx);
inline void unlockLevels(inode_t **nodes, int topLevel);