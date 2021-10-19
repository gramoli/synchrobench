#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <setjmp.h>
#include <stdint.h>
#include <unistd.h>
#include <vector>

#include "atomic_ops.h"

#define RECYCLED_VECTOR_RESERVE 5000000

#define MARK_BIT 1
#define FLAG_BIT 0

enum{INS,DEL};
enum {UNMARK,MARK};
enum {UNFLAG,FLAG};

typedef uintptr_t Word;

typedef struct node{
	int key;
	AO_double_t volatile child;
	#ifdef UPDATE_VAL
		long value;
	#endif
} node_t;

typedef struct seekRecord{
  // SeekRecord structure
  size_t leafKey;
  node_t * parent;
  AO_t pL;
  bool isLeftL; // is L the left child of P?
  node_t * lum;
  AO_t lumC;
  bool isLeftUM; // is  last unmarked node's child on access path the left child of  the last unmarked node?
} seekRecord_t;

typedef struct barrier {
	pthread_cond_t complete;
	pthread_mutex_t mutex;
	int count;
	int crossing;
} barrier_t;

typedef uintptr_t val_t;

typedef struct thread_data {
  val_t first;
  long range;
  int update;
  int alternate;
  int effective;
  int id;
  unsigned long numThreads;
  unsigned long nb_add;
  unsigned long nb_added;
  unsigned long nb_remove;
  unsigned long nb_removed;
  unsigned long nb_contains;
  unsigned long nb_found;
  unsigned long ops;
  unsigned int seed;
  double search_frac;
  double insert_frac;
  double delete_frac;
  long keyspace1_size;
  node_t* rootOfTree;
  barrier_t *barrier;
  std::vector<node_t *> recycledNodes;
  seekRecord_t * sr; // seek record
  seekRecord_t * ssr; // secondary seek record

} thread_data_t;


inline void *xmalloc(size_t size) {
  void *p = malloc(size);
  if (p == NULL) {
    perror("malloc");
    exit(1);
  }
  return p;
}


// Forward declaration of window transactions
int perform_one_delete_window_operation(thread_data_t* data, seekRecord_t * R, size_t key);

int perform_one_insert_window_operation(thread_data_t* data, seekRecord_t * R, size_t newKey);


/* ################################################################### *
 * Macro Definitions
 * ################################################################### */



inline bool SetBit(volatile unsigned long *array, int bit) {

     bool flag; 
     __asm__ __volatile__("lock bts %2,%1; setb %0" : "=q" (flag) : "m" (*array), "r" (bit)); return flag; 
   return flag;
}

bool mark_Node(volatile AO_t * word){
	return (SetBit(word, MARK_BIT));
}

#define atomic_cas_full(addr, old_val, new_val) __sync_bool_compare_and_swap(addr, old_val, new_val);


//-------------------------------------------------------------
#define create_child_word(addr, mark, flag) (((uintptr_t) addr << 2) + (mark << 1) + (flag))
#define is_marked(x) ( ((x >> 1) & 1)  == 1 ? true:false)
#define is_flagged(x) ( (x & 1 )  == 1 ? true:false)

#define get_addr(x) (x >> 2)
#define add_mark_bit(x) (x + 4UL)
#define is_free(x) (((x) & 3) == 0? true:false)

//-------------------------------------------------------------

/* ################################################################### *
 * Correctness Checking
 * ################################################################### */
size_t in_order_visit(node_t * rootNode){
	size_t key = rootNode->key;
	
	if((node_t *)get_addr(rootNode->child.AO_val1) == NULL){
		return (key);
	}
	
	node_t * lChild = (node_t *)get_addr(rootNode->child.AO_val1);
	node_t * rChild = (node_t *)get_addr(rootNode->child.AO_val2);
	
	if((lChild) != NULL){
		size_t lKey = in_order_visit(lChild);
		if(lKey >= key){
			std::cout << "Lkey is larger!!__" << lKey << "__ " << key << std::endl;
			std::cout << "Sanity Check Failed!!" << std::endl;
		}
	}
	
	if((rChild) != NULL){
	        size_t rKey = in_order_visit(rChild);
		if(rKey < key){
			std::cout << "Rkey is smaller!!__" << rKey << "__ " << key <<  std::endl;
			std::cout << "Sanity Check Failed!!" << std::endl;
		}
	}
	return (key);
}

