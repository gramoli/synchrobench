/* 
 * mytest.c: preliminary testing, written by Thomas Salemy, November 2018
 * 
 */

//NUMA Skip List Package
#define _GNU_SOURCE
#include "Set.h"
#include <sched.h>

//Testing Support
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <numa.h>
#include <atomic_ops.h>

#define MAX_NUMA_ZONES          numa_max_node() + 1
#define MIN_NUMA_ZONES          1

int stop_condition = 0;

//Numa Skip List Requirements
extern searchLayer_t** numaLayers;
extern numa_allocator_t** allocators;
extern int numberNumaZones;
extern unsigned int levelmax;
extern dataLayerThread_t* remover; 
typedef struct zone_init_args {
  int     numa_zone;
  node_t*   head;
  node_t*   tail;
  unsigned  allocator_size;
} zone_init_args_t;

void initializeDefault() {

}

//Must Initialize In Order
//0) Set Numa Zone to Requested Zone
//1) Allocator
//2) Index Node Head / Tail Sentinel
//3) Search Layer
void* zone_init(void* args) {
  zone_init_args_t* zia = (zone_init_args_t*)args;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(zia -> numa_zone, &cpuset);
  sleep(1);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  numa_set_preferred(zia -> numa_zone);

  allocators[zia -> numa_zone] = constructAllocator(zia -> allocator_size);
  inode_t* tail = constructIndexNode(INT_MAX, levelmax, zia -> tail, zia -> numa_zone);
  inode_t* head = constructLinkedIndexNode(INT_MIN, levelmax, zia -> head, zia -> numa_zone, tail);
  numaLayers[zia -> numa_zone] = constructSearchLayer(head, zia -> numa_zone);
  free(zia);
  return NULL;
}


typedef struct thread_data {
  searchLayer_t* numask;
} thread_data_t;

void* test1(void* data) {
  thread_data_t* data = (thread_data_t*)data;
  searchLayer_t* numask = data -> numask;
  for (int i = 0; i < 24; i++) {
    sl_add(numask, rand() % 100);
  }

  for (int i = 0; i < 24; i++) {
    sl_remove(numask, rand() % 100);
  }

  for (int i = 0; i < 24; i++) {
    sl_contains(numask, rand() % 100);
  }
}


//Initialize In the Following Order
//1) Levelmax of all searchlayers
//2) numberNumaZones to be used
//3) Data Layer Sentinels (head + tail)
//4) Allocator Array (allocators)
//4) IndexLayer Array (numaLayers)
//5) IndexLayers using Sentinel Nodes
//
//6) Data Layer Helper Thread
//7) Search Layer Helper Threads
//
//8) Finally Initialize Application Threads

int main(int argc, char** argv) {
  int initial = 100;
  int numThreads = 2;
  levelmax = floor_log_2((unsigned int) initial);
  numberNumaZones = MAX_NUMA_ZONES;

  //create sentinel node on NUMA zone 0
  node_t* tail = constructNode(INT_MAX, numberNumaZones);
  node_t* head = constructLinkedNode(INT_MIN, numberNumaZones, tail);
  printf("Created Data Layer Sentinels\n");

  //create allocators
  allocators = (numa_allocator_t**)malloc(numberNumaZones * sizeof(numa_allocator_t*));
  printf("Created Array of Allocators\n");

  //create search layers
  numaLayers = (searchLayer_t**)malloc(numberNumaZones * sizeof(searchLayer_t*));
  printf("Created Array of Numa Layers \n");

  unsigned num_expected_nodes = (unsigned)(16 * initial * (1.0 + (33 / 100.0)));
  unsigned buffer_size = CACHE_LINE_SIZE * num_expected_nodes;

  //initialize search layers
  pthread_t* thds = (pthread_t*)malloc(numberNumaZones * sizeof(pthread_t));
  for (int i = 0; i < numberNumaZones; ++i) {
    zone_init_args_t* zia = (zone_init_args_t*)malloc(sizeof(zone_init_args_t));
    zia -> numa_zone = i;
    zia -> head = head;
    zia -> tail = tail;
    zia -> allocator_size = buffer_size;
    pthread_create(&thds[i], NULL, zone_init, (void*)zia);
  }
  for (int i = 0; i < numberNumaZones; ++i) {
    pthread_join(thds[i], NULL);
  }
  printf("Initialized search layers\n");

  //start data-layer-helper thread
  char test_complete = 0;
  startDataLayerThread(head);

  //start pernuma layer helper with 100000ms sleep time
  for(int i = 0; i < numberNumaZones; ++i) {
    start(numaLayers[i], 100000);
  }

  int initialSize = sl_size(head);
  printf("Initial Size of: %d\n", initialSize);
  stop_condition = 0;

  //initialize application threads
  pthread_t *threads = (pthread_t *)malloc(numThreads * sizeof(pthread_t));
  thread_data_t* data = (thread_data_t *)malloc(numThreads * sizeof(thread_data_t));
  for (int i = 0; i < numThreads; i++) {
    data[i].numask = numaLayers[i % numberNumaZones];
    pthread_create(&threads[i], NULL, test, (void*)(&data[i]));
  }
  printf("Initialized all application threads\n");

  for (int i = 0; i < numThreads; i++) {
    printf("Applicaiton thread %d completed\n", i);
    pthread_join(threads[i], NULL);
  }
  printf("All application threads joined\n");

  node_t* runner = head;
  while (runner != NULL) {
    printf("%d ", runner -> val);
  }
  printf("\n");
  printf("Completed Test\n");
}