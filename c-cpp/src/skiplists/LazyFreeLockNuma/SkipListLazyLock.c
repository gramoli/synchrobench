#ifndef SKIPLISTLAZYLOCK_C
#define SKIPLISTLAZYLOCK_C

#include "SkipListLazyLock.h"
#include "Atomic.h"
#include "Hazard.h"

//Inserts a value into the skip list if it doesn't already exist
int add(inode_t *sentinel, int val, node_t* dataLayer, int zone) {
  //store the result of a traversal through the skip list while searching for the target
  inode_t *predecessors[sentinel -> topLevel], *successors[sentinel -> topLevel];
  inode_t *previous = sentinel, *current = NULL;

  //Lazily searches the skip list, disregarding locks
  for (int i = previous -> topLevel - 1; i >= 0; i--) {
    current = previous -> next[i];
    while (current -> val < val) {
      previous = current;
      current = previous -> next[i];
    }
    predecessors[i] = previous;
    successors[i] = current;
  }

  //if the value doesn't already exist, create a new index node and return true
  //otherwise return false
  inode_t* candidate = current;
  if (candidate -> val != val) {
    const int topLevel = getRandomLevel(sentinel -> topLevel);
    inode_t* insertion = constructIndexNode(val, topLevel, dataLayer, zone);
    for (int i = 0; i < topLevel; i++) {
      insertion -> next[i] = successors[i];
      predecessors[i] -> next[i] = insertion;
    }
    return 1;
  }
  return 0;
}

//removes a value in the skip list when present
int removeNode(inode_t *sentinel, int val, int zone) {
  //store the result of a traversal through the skip list while searching for a value
  inode_t *predecessors[sentinel -> topLevel], *successors[sentinel -> topLevel];
  inode_t *previous = sentinel, *current = NULL;

  //Lazily searches the skip list, disregarding locks
  for (int i = previous -> topLevel - 1; i >= 0; i--) {
    current = previous -> next[i];
    while (current -> val < val) {
      previous = current;
      current = previous -> next[i];
    }
    predecessors[i] = previous;
    successors[i] = current;
  }

  //if the node contains the targeted value, 
  //remove the node, decrement the data layer references value, and return true
  //otherwise return false
  inode_t* candidate = current;
  if (candidate -> val == val) {
    for (int i = 0; i < candidate -> topLevel; i++) {
        predecessors[i] -> next[i] = successors[i] -> next[i];
    }
    return 1;
  }
  return 0;
}

#endif