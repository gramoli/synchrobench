#ifndef SKIPLISTLAZYLOCK_C
#define SKIPLISTLAZYLOCK_C

#include "SkipListLazyLock.h"
#include "Atomic.h"
#include "Profiler.h"

//Inserts a value into the skip list if it doesn't already exist
int add(inode_t *sentinel, int val, node_t* dataLayer, int zone, volatile long* bg_local_accesses, volatile long* bg_foreign_accesses, int index_ignore) {
  //store the result of a traversal through the skip list while searching for the target
  inode_t *predecessors[sentinel -> topLevel], *successors[sentinel -> topLevel];
  inode_t *previous = sentinel, *current = NULL;
  zone_access_check(zone, previous, bg_local_accesses, bg_foreign_accesses, index_ignore);

  //Lazily searches the skip list, disregarding locks
  for (int i = previous -> topLevel - 1; i >= 0; i--) {
    current = previous -> next[i];
    zone_access_check(zone, current, bg_local_accesses, bg_foreign_accesses, index_ignore);
    while (current -> val < val) {
      previous = current;
      current = previous -> next[i];
      zone_access_check(zone, current, bg_local_accesses, bg_foreign_accesses, index_ignore);
    }
    predecessors[i] = previous;
    successors[i] = current;
  }

  //if the value doesn't already exist, create a new index node and return true
  //otherwise return false
  inode_t* candidate = current;
  if (candidate -> val != val) {
    const int topLevel = getRandomLevel(sentinel -> topLevel);
    zone_access_check(zone, dataLayer, bg_local_accesses, bg_foreign_accesses, index_ignore);
    inode_t* insertion = constructIndexNode(val, topLevel, dataLayer, zone);
    zone_access_check(zone, insertion, bg_local_accesses, bg_foreign_accesses, index_ignore);
    for (int i = 0; i < topLevel; i++) {
      insertion -> next[i] = successors[i];
      predecessors[i] -> next[i] = insertion;
    }
    return 1;
  }
  return 0;
}

//removes a value in the skip list when present
int removeNode(inode_t *sentinel, int val, int zone, volatile long* bg_local_accesses, volatile long* bg_foreign_accesses, int index_ignore) {
  //store the result of a traversal through the skip list while searching for a value
  inode_t *predecessors[sentinel -> topLevel], *successors[sentinel -> topLevel];
  inode_t *previous = sentinel, *current = NULL;
  zone_access_check(zone, previous, bg_local_accesses, bg_foreign_accesses, index_ignore);

  //Lazily searches the skip list, disregarding locks
  for (int i = previous -> topLevel - 1; i >= 0; i--) {
    current = previous -> next[i];
    zone_access_check(zone, current, bg_local_accesses, bg_foreign_accesses, index_ignore);
    while (current -> val < val) {
      previous = current;
      current = previous -> next[i];
      zone_access_check(zone, current, bg_local_accesses, bg_foreign_accesses, index_ignore);
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
        zone_access_check(zone, successors[i] -> next[i], bg_local_accesses, bg_foreign_accesses, index_ignore);
    }
    zone_access_check(zone, candidate -> dataLayer, bg_local_accesses, bg_foreign_accesses, index_ignore);
    FAD(&candidate -> dataLayer -> references); // Question: is this correct?
    //candidate -> dataLayer = NULL; //QUESTION: will this be a problem and is it needed?
    return 1;
  }
  return 0;
}

#endif