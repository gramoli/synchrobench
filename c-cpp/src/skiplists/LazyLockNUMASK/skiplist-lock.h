#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <atomic_ops.h>

#define INT_MIN -2147483648
#define INT_MAX 2147483647

typedef struct node {
  int val; //stores the value at the node
  int topLevel; //stores the height of the tower
  struct node** next; //stores the next pointer for each level in the tower
  volatile int markedToDelete; //stores whether marked for deletion
  volatile int fullylinked; //stores whether fullylinked or in the middle of insertion
  pthread_mutex_t lock; //node-specific mutex
} node_t;

node_t* constructNode(int val, int topLevel);
node_t* constructLinkedNode(int val, int topLevel, node_t* next);
void destructNode(node_t* node);

typedef struct SkipList {
  struct node* head; //stores the head sentinel node
  int maxLevel; //stores the max number of levels, declared at runtime by user
} skipList_t;

skipList_t* constructSkipList(int maxLevel);
skipList_t* destructSkipList(skipList_t* skipList);
size_t getSize(skipList_t* skipList);

//Helper Methods
int floor_log_2(unsigned int n);
int getRandomLevel(unsigned int maxLevel);
