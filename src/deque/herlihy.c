/*
 * File:
 *   herlihy.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Obstruction-free double-ended queue extended to circular arrays
 *	 "Obstruction-Free Synchronization: Double-Ended Queues as an Example"
 *	 M. Herlihy, V. Luchangco, M. Moir, p.522, ICDCS 2003.
 * 
 * Copyright (c) 2008-2009.
 *
 * herlihy.c is part of Synchrobench
 * 
 * Synchrobench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "herlihy.h"

val_t lhint;
val_t rhint;

void printdq(circarr_t *arr) {
	int size, i, middle = MAX/2;
	
	for (i=0; i <= MAX+1; i++) {
	  if (i == middle) {
	    
	    switch (arr[i]->val) {
	    case LN:
	      printf("** <LN, %d> **", arr[i]->ctr); break;
	    case RN:
	      printf("** <RN, %d> **", arr[i]->ctr); break;
	    case DN:
	      printf("** <DN, %d> **", arr[i]->ctr); break;
	    default:
	      printf("** <%d, %d> **", arr[i]->val, arr[i]->ctr); 
	    }
	    
	  } else {
	    
	    switch (arr[i]->val) {
	    case LN:
	      printf("<LN, %d>", arr[i]->ctr); break;	
	    case RN:
	      printf("<RN, %d>", arr[i]->ctr); break;
	    case DN:
	      printf("<DN, %d>", arr[i]->ctr); break;
	    default:
	      printf("<%d, %d>", arr[i]->val, arr[i]->ctr);	
	    }
	    
	  }
	  if (i<MAX+1) printf(" - ");
	}
	size = i-2;
	printf("\n");
}

int lookup(circarr_t *arr, int val) {
	int i;
	for (i=0; i <= MAX+1; i++) {
		if (arr[i]->val == val) break;
	}
	return i;
}

/*
 * Eventually accurate oracle (when executing in isolation for long enough)
 * returns left-most RN (resp. right-most LN) when invoked with RIGHT (resp. LEFT)
 */
int oracle(circarr_t *arr, int direction) {
	int i = -1;
	
	while(i < 0 || i > MAX+1) {
		if (direction == RIGHT) {
			for (i = MAX+1; i >= 0; i--) {
				if ((arr[i]->val == RN || arr[i]->val == DN) && arr[(MAX+i+1) % (MAX+2)]->val != RN)
					return i;
			}
		} else { // direction == LEFT
			for (i = 0; i <= MAX+1; i++) {
				if ((arr[i]->val == LN || arr[i]->val == DN) && arr[(i+1) % (MAX+2)]->val != LN)
					return i;
			}	
		}
	}
	return -1;
}

/*
 * Calls oracle RIGHT
 * Returns k and sets left and right where *left = A[k-1] at some time t, and 
 * *right = A[k] at some time t'>t during the execution, with (*left)->val != RN
 * and (*right)->val == RN.
 *
 * TODO: The counter should be part of the same memory word as the node itself
 * so that a CAS succeeds only if the counter has not been modified,
 */
int rightcheckedoracle(circarr_t *arr, node_t **left, node_t **right) {
	int k; 
	node_t *new, *new2;
	
	new = (node_t *)malloc(sizeof(node_t));
	new2 = (node_t *)malloc(sizeof(node_t));
	while (1) { 
		k = oracle(arr, RIGHT); 
		(*left) = arr[(k+MAX+1) % (MAX+2)]; // order important for check 
		(*right) = arr[k]; // for empty in rightpop 
		if ((*right)->val == RN && (*left)->val != RN) 
			// correct oracle 
			return k; 
		if ((*right)->val == DN && (*left)->val != RN && (*left)->val != DN) { 
			// correct oracle, but no RNs 
			new->val = (*left)->val;
			new->ctr = (*left)->ctr+1;
			if (ATOMIC_CAS_MB(&arr[(k+MAX+1) % (MAX+2)]->val, (*left)->val, new))  {	
			  //if (ATOMIC_CAS_MB(&arr[(k+MAX+1) % (MAX+2)], (*left), new))  {	
				new2->val = RN;
				new2->ctr = (*right)->ctr+1;	
				if (ATOMIC_CAS_MB(&arr[k]->val, (*right)->val, new2)) { 
				//if (ATOMIC_CAS_MB(&arr[k], (*right), new2)) { 
					// DN -> RN
					(*left)->ctr++;
					(*right)->val = RN;
					(*right)->ctr++;
					return k; 
				}
			}
		}
	} // end while 
}


/*
 * Calls oracle LEFT
 * Returns k and sets right and left where *right = A[k+1] at some time t, and 
 * *left = A[k] at some time t'>t during the execution, with (*right)->val != LN
 * and (*left)->val == LN.
 */
int leftcheckedoracle(circarr_t *arr, node_t **right, node_t **left) {
	int k;
	node_t *new, *new2;
	
	new = (node_t *)malloc(sizeof(node_t));
	new2 = (node_t *)malloc(sizeof(node_t));
	while (1) { 
		k = oracle(arr, LEFT); 
		(*right) = arr[(k+1) % (MAX+2)]; // order important for check 
		(*left) = arr[k]; // for empty in rightpop 
		if ((*left)->val == LN && (*right)->val != LN) 
			// correct oracle 
			return k; 
		if ((*left)->val == DN && (*right)->val != LN && (*right)->val != DN) { // correct oracle, but no LNs 
			new->val = (*right)->val;
			new->ctr = (*right)->ctr+1;
			if (ATOMIC_CAS_MB(&arr[(k+1) % (MAX+2)]->val, (*right)->val, new))  {	
			  //if (ATOMIC_CAS_MB(&arr[(k+1) % (MAX+2)], (*right), new))  {	
				new2->val = LN;
				new2->ctr = (*left)->ctr+1;		
				if (ATOMIC_CAS_MB(&arr[k]->val, (*left)->val, new2)) { 
				  //if (ATOMIC_CAS_MB(&arr[k], (*left), new2)) { 
					// DN -> LN
					(*right)->ctr++;
					(*left)->val = LN;
					(*left)->ctr++;
					return k; 
				}
			}
		}
	} // end while 
}



/*
 * Right push val in the circular array
 * Returns 0 if full, returns 1 otherwise
 */
int herlihy_rightpush(circarr_t *arr, val_t val, int transactional) { 
	// !(val in {LN,RN,DN}) 
	int k;
	node_t *prev, *cur, *next, *nextnext, *new, *new2, *new3, *new4, *new5, *new6;
	
	prev = (node_t *)malloc(sizeof(node_t));
	cur = (node_t *)malloc(sizeof(node_t));
	new = (node_t *)malloc(sizeof(node_t));
	new2 = (node_t *)malloc(sizeof(node_t));
	new3 = (node_t *)malloc(sizeof(node_t));
	new4 = (node_t *)malloc(sizeof(node_t));
	new5 = (node_t *)malloc(sizeof(node_t));
	new6 = (node_t *)malloc(sizeof(node_t));
	while (1) { 
		k = rightcheckedoracle(arr, &prev, &cur); 
		next = arr[(k+1) % (MAX+2)]; 
		if (next) {
			if (next->val == RN) {
				new->val = prev->val;
				new->ctr = prev->ctr+1;
				if (ATOMIC_CAS_MB(&arr[(k+MAX+1) % (MAX+2)], prev, new)) {
					new2->val = val;
					new2->ctr = cur->ctr+1;
					if (ATOMIC_CAS_MB(&arr[k], cur, new2)) // RN -> val 
						return 1; 
				}
			}
			if (next->val == LN) {
				new5->val = RN;
				new5->ctr = cur->ctr+1;
				if (ATOMIC_CAS_MB(&arr[k], cur, new5)) {
					new6->val = DN;
					new6->ctr = next->ctr+1;
					ATOMIC_CAS_MB(&arr[(k+1) % (MAX+2)], next, new6); // LN -> DN
				}
			}
			if (next->val == DN) { 
				nextnext = arr[(k+2) % (MAX+2)]; 
				if (nextnext) {
				       if (nextnext->val != RN && 
					    nextnext->val != LN && nextnext->val != DN) {
					  //if (arr[(k+MAX+1) % (MAX+2)] == prev) {
					  if (arr[(k+MAX+1) % (MAX+2)]->val == prev->val) {
					    if (arr[k] == cur) return 0; // full 
					  }
					}
					if (nextnext->val == LN) {
						new3->val = nextnext->val;
						new3->ctr = nextnext->ctr+1;
						if (ATOMIC_CAS_MB(&arr[(k+2) % (MAX+2)], nextnext, new3)) { 
							new4->val = RN;
							new4->ctr = next->ctr+1;
							ATOMIC_CAS_MB(&arr[(k+1) % (MAX+2)], next, new4); // DN -> RN 
						}
					}
				} else return 0; // nextnext is null
			} // end if (next->val = DN) 
		} else return 0; // next is null
	} //end while 
}
		
/*
 * Left push val in the circular array
 * Returns 0 if full, returns 1 otherwise
 */
int herlihy_leftpush(circarr_t *arr, val_t val, int transactional) { 
	// !(val in {LN,RN,DN}) 
	int k;
	node_t *prev, *cur, *next, *nextnext, *new, *new2, *new3, *new4, *new5, *new6;
	
	prev = (node_t *)malloc(sizeof(node_t));
	cur = (node_t *)malloc(sizeof(node_t));
	new = (node_t *)malloc(sizeof(node_t));
	new2 = (node_t *)malloc(sizeof(node_t));
	new3 = (node_t *)malloc(sizeof(node_t));
	new4 = (node_t *)malloc(sizeof(node_t));
	new5 = (node_t *)malloc(sizeof(node_t));
	new6 = (node_t *)malloc(sizeof(node_t));
	while (1) { 
		k = leftcheckedoracle(arr, &prev, &cur); 
		// cur->val = LN and prev->val != LN 
		next = arr[(k+MAX+1) % (MAX+2)]; 
		if (next->val == LN) {
			new->val = prev->val;
			new->ctr = prev->ctr+1;
			if (ATOMIC_CAS_MB(&arr[(k+1) % (MAX+2)], prev, new)) {
				new2->val = val;
				new2->ctr = cur->ctr+1;
				if (ATOMIC_CAS_MB(&arr[k], cur, new2)) // LN -> val 
					return 1; 
			}
		}
		if (next->val == RN) {
			new3->val = LN;
			new3->ctr = cur->ctr+1;
			if (ATOMIC_CAS_MB(&arr[k], cur, new3)) {
				new4->val = DN;
				new4->ctr = next->ctr+1;
				ATOMIC_CAS_MB(&arr[(k+MAX+1) % (MAX+2)], next, new4); // RN -> DN
			}
		}
		if (next->val == DN) { 
			nextnext = arr[(k+MAX) % (MAX+2)]; 
			if (nextnext->val != LN && 
				nextnext->val != RN && nextnext->val != DN) {
			  //if (arr[(k+1) % (MAX+2)] == prev) {
			  if (arr[(k+1) % (MAX+2)]->val == prev->val) {	  
			    //if (arr[k] == cur) return 0;
			    if (arr[k]->val == cur->val) return 0;		
			  }
			}
			if (nextnext->val == RN) {
				new5->val = nextnext->val;
				new5->ctr = nextnext->ctr+1;
				if (ATOMIC_CAS_MB(&arr[(k+MAX) % (MAX+2)], nextnext, new5)) { 
					new6->val = LN;
					new6->ctr = next->ctr+1;
					ATOMIC_CAS_MB(&arr[(k+MAX+1) % (MAX+2)], next, new6); // DN -> LN 
				}
			}
		} // end if (next->val = DN) 
	} //end while 
}


/*
 * Right pop for the circular array
 * Returns 0 if empty, returns the poped value otherwise
 */
int herlihy_rightpop(circarr_t *arr, int transactional) {
	int k;
	node_t *cur, *next, *new, *new2;
					
	new = (node_t *)malloc(sizeof(node_t));	
	new2 = (node_t *)malloc(sizeof(node_t));
	while (1) { 
		k = rightcheckedoracle(arr, &cur, &next); 
	    		
		//if ((cur->val == LN || cur->val == DN) && arr[(k+MAX+1) % (MAX+2)]->val == cur->val) { 
		if ((cur->val == LN || cur->val == DN) && arr[(k+MAX+1) % (MAX+2)] == cur) { 
			return 0; // empty
		}
		new->val = RN;
		new->ctr = new->ctr+1;
		if (ATOMIC_CAS_MB(&arr[k], next, new)) {
			new2->val = RN;
			new2->ctr = cur->ctr+1;
			if (ATOMIC_CAS_MB(&arr[(k+MAX+1) % (MAX+2)], cur, new2)) { // v -> RN 
			  return cur->val; 
			}
		}
	} // end while 
}		


/*
 * Left pop for the circular array
 * Returns 0 if empty, returns the poped value otherwise
 */
int herlihy_leftpop(circarr_t *arr, int transactional) {
	int k;
	node_t *cur, *next, *new, *new2;
	
	new = (node_t *)malloc(sizeof(node_t));
	new2 = (node_t *)malloc(sizeof(node_t));
	while (1) { // keep trying till return val or empty 
		k = oracle(arr, LEFT); // find index of leftmost RN 
		cur = arr[(k+1) % (MAX+2)]; // read (supposed) value to be popped 
		next = arr[k]; // read (supposed) leftmost RN 
		if (cur->val != LN && next->val == LN) { // oracle is right 
			if (cur->val == RN && arr[(k+1) % (MAX+2)] == cur) // adjacent LN and RN 
				return 0; 
			new->val = LN;
			new->ctr = next->ctr+1;
			if (ATOMIC_CAS_MB(&arr[k], next, new)) {
				// try to bump up next->ctr 
				new2->val = LN;
				new2->ctr = cur->ctr+1;
				if (ATOMIC_CAS_MB(&arr[(k+1) % (MAX+2)], cur, new2)) {
		                  // try to remove value 
				  return cur->val; // it worked; return removed value 
				}
			}
		} // end if (cur->val != RN and next->val = RN) 
	} // end while 
}



/*
 * Here we give the version with markers at both end of the deque.
 *
 * The hint version uses an oracle that keeps track of leftmost  
 * and rightmost markers to indicate where is the left end and 
 * right end of the deque.
 *
 * TODO: The counter should be part of the same memory word as the node itself
 * so that a CAS succeeds only if the counter has not been modified.
 */


/*
 * Sets atomically the left or right hints that indicate
 * the left and right ends (resp.) of the dequeue.
 */
inline void set_hint(val_t *hintad, val_t offset) {
  val_t val, new_val;

  do {
    val = *hintad;
    new_val = (val+offset) % (MAX+2);
  } while (!ATOMIC_CAS_MB(hintad, val, new_val));
}


/*
 * Calls oracle RIGHT
 * Returns k and sets left and right where *left = A[k-1] at some time t, and 
 * *right = A[k] at some time t'>t during the execution, with (*left)->val != RN
 * and (*right)->val == RN.
 */
int rightcheckedoracle_hint(circarr_t *arr, node_t **left, node_t **right) {
  int k; 
  node_t *new, *new2;
  
  new = (node_t *)malloc(sizeof(node_t));
  new2 = (node_t *)malloc(sizeof(node_t));
  while (1) {
    k = rhint;    
    (*left) = arr[(k+MAX+1) % (MAX+2)]; // order important for check 
    (*right) = arr[k]; // for empty in rightpop 
    if ((*right)->val == RN && (*left)->val != RN) 
      // correct oracle 
      return k; 
    
    if ((*right)->val == DN && (*left)->val != RN && (*left)->val != DN) {
      // correct oracle, but no RNs 
      new->val = (*left)->val;
      new->ctr = (*left)->ctr+1;
      //if (ATOMIC_CAS_MB(&arr[(k+MAX+1) % (MAX+2)]->val, (*left)->val, new))  {
      if (ATOMIC_CAS_MB(&arr[(k+MAX+1) % (MAX+2)], (*left), new))  {	
	new2->val = RN;
	new2->ctr = (*right)->ctr+1;	
	//if (ATOMIC_CAS_MB(&arr[k]->val, (*right)->val, new2)) { 
	if (ATOMIC_CAS_MB(&arr[k], (*right), new2)) { 
	  // DN -> RN
	  (*left)->ctr++;
	  (*right)->val = RN;
	  (*right)->ctr++;
	  return k; 
	}
      }
    }
  } // end while 
}


int leftcheckedoracle_hint(circarr_t *arr, node_t **right, node_t **left) {
  int k;
	node_t *new, *new2;
	
	new = (node_t *)malloc(sizeof(node_t));
	new2 = (node_t *)malloc(sizeof(node_t));
	while (1) {
	  k = lhint;
		(*right) = arr[(k+1) % (MAX+2)]; // order important for check 
		(*left) = arr[k]; // for empty in rightpop 
		if ((*left)->val == LN && (*right)->val != LN) 
			// correct oracle 
			return k; 
		if ((*left)->val == DN && (*right)->val != LN && (*right)->val != DN) { // correct oracle, but no LNs 
			new->val = (*right)->val;
			new->ctr = (*right)->ctr+1;
			//if (ATOMIC_CAS_MB(&arr[(k+1) % (MAX+2)]->val, (*right)->val, new))  {	
			if (ATOMIC_CAS_MB(&arr[(k+1) % (MAX+2)], (*right), new))  {	
				new2->val = LN;
				new2->ctr = (*left)->ctr+1;	
				//if (ATOMIC_CAS_MB(&arr[k]->val, (*left)->val, new2)) { 
				if (ATOMIC_CAS_MB(&arr[k], (*left), new2)) { 
					// DN -> LN
					(*right)->ctr++;
					(*left)->val = LN;
					(*left)->ctr++;
					return k; 
				}
			}
		}
	} // end while 
}



/*
 * Right push val in the circular array
 * Returns 0 if full, returns 1 otherwise
 */
int herlihy_rightpush_hint(circarr_t *arr, val_t val, int transactional) { 
  // !(val in {LN,RN,DN}) 
  int k;
  node_t *prev, *cur, *next, *nextnext, *new, *new2, *new3, *new4, *new5, *new6;
  
  prev = (node_t *)malloc(sizeof(node_t));
  cur = (node_t *)malloc(sizeof(node_t));
  new = (node_t *)malloc(sizeof(node_t));
  new2 = (node_t *)malloc(sizeof(node_t));
  new3 = (node_t *)malloc(sizeof(node_t));
  new4 = (node_t *)malloc(sizeof(node_t));
  new5 = (node_t *)malloc(sizeof(node_t));
  new6 = (node_t *)malloc(sizeof(node_t));
  while (1) { 
    k = rightcheckedoracle_hint(arr, &prev, &cur); 
    next = arr[(k+1) % (MAX+2)]; 
    if (next) {
      if (next->val == RN) {
	new->val = prev->val;
	new->ctr = prev->ctr+1;
	if (ATOMIC_CAS_MB(&arr[(k+MAX+1) % (MAX+2)], prev, new)) {
	  new2->val = val;
	  new2->ctr = cur->ctr+1;
	  //if (ATOMIC_CAS_MB(&arr[k]->val, cur->val, new2)) { // RN -> val
	  if (ATOMIC_CAS_MB(&arr[k], cur, new2)) { // RN -> val 
	    set_hint(&rhint, 1);
	    return 1; 
	  }
	}
      }
      if (next->val == LN) {
	new5->val = RN;
	new5->ctr = cur->ctr+1;
	if (ATOMIC_CAS_MB(&arr[k], cur, new5)) {
	  new6->val = DN;
	  new6->ctr = next->ctr+1;
	  ATOMIC_CAS_MB(&arr[(k+1) % (MAX+2)], next, new6); // LN -> DN
	}
      }
      if (next->val == DN) { 
	nextnext = arr[(k+2) % (MAX+2)]; 
	if (nextnext) {
	  if (nextnext->val != RN && 
	      nextnext->val != LN && nextnext->val != DN) {
	    //if (arr[(k+MAX+1)%(MAX+2)] == prev) {
	      if (arr[(k+MAX+1)%(MAX+2)]->val == prev->val) {
		//if (arr[k] == cur) return 0; // full
	      if (arr[k]->val == cur->val) return 0; // full
	    }
	  }
	  if (nextnext->val == LN) {
	    new3->val = nextnext->val;
	    new3->ctr = nextnext->ctr+1;
	    if (ATOMIC_CAS_MB(&arr[(k+2) % (MAX+2)], nextnext, new3)) { 
	      new4->val = RN;
	      new4->ctr = next->ctr+1;
	      ATOMIC_CAS_MB(&arr[(k+1) % (MAX+2)], next, new4); // DN -> RN 
	    }
	  }
	} else return 0; // nextnext is null
      } // end if (next->val = DN) 
    } else return 0; // next is null
  } //end while 
}

/*
 * Left push val in the circular array
 * Returns 0 if full, returns 1 otherwise
 */
int herlihy_leftpush_hint(circarr_t *arr, val_t val, int transactional) { 
	// !(val in {LN,RN,DN}) 
	int k;
	node_t *prev, *cur, *next, *nextnext, *new, *new2, *new3, *new4, *new5, *new6;
	
	prev = (node_t *)malloc(sizeof(node_t));
	cur = (node_t *)malloc(sizeof(node_t));
	new = (node_t *)malloc(sizeof(node_t));
	new2 = (node_t *)malloc(sizeof(node_t));
	new3 = (node_t *)malloc(sizeof(node_t));
	new4 = (node_t *)malloc(sizeof(node_t));
	new5 = (node_t *)malloc(sizeof(node_t));
	new6 = (node_t *)malloc(sizeof(node_t));
	while (1) { 
		k = leftcheckedoracle_hint(arr, &prev, &cur); 
		// cur->val = LN and prev->val != LN 
		next = arr[(k+MAX+1) % (MAX+2)]; 
		if (next->val == LN) {
			new->val = prev->val;
			new->ctr = prev->ctr+1;
			if (ATOMIC_CAS_MB(&arr[(k+1) % (MAX+2)], prev, new)) {
				new2->val = val;
				new2->ctr = cur->ctr+1;
				//if (ATOMIC_CAS_MB(&arr[k]->val, cur->val, new2)) { // LN -> val 
			  if (ATOMIC_CAS_MB(&arr[k], cur, new2)) { // LN -> val 
				  set_hint(&lhint, MAX+1);
				  return 1; 
				}
			}
		}
		if (next->val == RN) {
			new3->val = LN;
			new3->ctr = cur->ctr+1;
			if (ATOMIC_CAS_MB(&arr[k], cur, new3)) {
				new4->val = DN;
				new4->ctr = next->ctr+1;
				ATOMIC_CAS_MB(&arr[(k+MAX+1) % (MAX+2)], next, new4); // RN -> DN
			}
		}
		if (next->val == DN) { 
		  nextnext = arr[(k+MAX) % (MAX+2)]; 
			if (nextnext->val != LN && 
				nextnext->val != RN && nextnext->val != DN) {
			  //if (arr[(k+1) % (MAX+2)] == prev) {
			  //		if (arr[k] == cur) return 0; 
				if (arr[(k+1) % (MAX+2)]->val == prev->val) {
					if (arr[k]->val == cur->val) return 0; 
				}
			}
			if (nextnext->val == RN) {
				new5->val = nextnext->val;
				new5->ctr = nextnext->ctr+1;
				if (ATOMIC_CAS_MB(&arr[(k+MAX) % (MAX+2)], nextnext, new5)) { 
					new6->val = LN;
					new6->ctr = next->ctr+1;
					ATOMIC_CAS_MB(&arr[(k+MAX+1) % (MAX+2)], next, new6); // DN -> LN 
				}
			}
		} // end if (next->val = DN) 
	} //end while 
}


/*
 * Right pop for the circular array
 * Returns 0 if empty, returns the poped value otherwise
 */
int herlihy_rightpop_hint(circarr_t *arr, int transactional) {
	int k;
	node_t *cur, *next, *new, *new2;
					
	new = (node_t *)malloc(sizeof(node_t));	
	new2 = (node_t *)malloc(sizeof(node_t));
	while (1) { 
		k = rightcheckedoracle_hint(arr, &cur, &next); 
		if ((cur->val == LN || cur->val == DN) && arr[(k+MAX+1) % (MAX+2)]->val == cur->val) { 
		  //if ((cur->val == LN || cur->val == DN) && arr[(k+MAX+1) % (MAX+2)] == cur) { 
			return 0; // empty
		}
		new->val = RN;
		new->ctr = new->ctr+1;
		if (ATOMIC_CAS_MB(&arr[k], next, new)) {
			new2->val = RN;
			new2->ctr = cur->ctr+1;
			if (ATOMIC_CAS_MB(&arr[(k+MAX+1) % (MAX+2)], cur, new2)) { // v -> RN
			  set_hint(&rhint, MAX+1);
			  return cur->val; 
			}
		}
	} // end while 
}		

/*
 * Left pop for the circular array
 * Returns 0 if empty, returns the poped value otherwise
 */
int herlihy_leftpop_hint(circarr_t *arr, int transactional) {
  int k;
  node_t *cur, *next, *new, *new2;
  
  new = (node_t *)malloc(sizeof(node_t));
  new2 = (node_t *)malloc(sizeof(node_t));
  while (1) { // keep trying till return val or empty 
    k = leftcheckedoracle_hint(arr, &cur, &next); 
    //k = oracle(arr, LEFT); cur = arr[(k+MAX+1) % (MAX+2)]; next = arr[k];
    if (cur->val != LN && next->val == LN) { // oracle is right 
      if (cur->val == RN && arr[(k+1) % (MAX+2)] == cur) // adjacent LN and RN 
	return 0; 
      new->val = LN;
      new->ctr = next->ctr+1;
      if (ATOMIC_CAS_MB(&arr[k], next, new)) {
	// try to bump up next->ctr 
	new2->val = LN;
	new2->ctr = cur->ctr+1;
	if (ATOMIC_CAS_MB(&arr[(k+1) % (MAX+2)], cur, new2)) { 
	  // try to remove value 
	  set_hint(&lhint, 1);
	  return cur->val; // it worked; return removed value 
	}
      }
    } // end if (cur->val != RN and next->val = RN) 
  } // end while 
}
