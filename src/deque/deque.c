/*
 * File:
 *   deque.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Double-ended queue.
 * 
 * Copyright (c) 2008-2009.
 *
 * deque.c is part of Synchrobench
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

#include "deque.h"

val_t lhint;
val_t rhint;

int seq_rightpush(circarr_t *queue, val_t val, int transactional) {
  int result = 0;
  int i;
  
  i = rhint;
  if (queue[(i+1) % (MAX+2)]->val == RN) {
    /* we can push */
    queue[i]->val = val;
    rhint = (rhint+1) % (MAX+2);
    result = 1;
  } else {
    /* full: cannot push */
    result = 0; 
  }
  return result;
}


int seq_leftpush(circarr_t *queue, val_t val, int transactional) {
  int result = 0;
  int i;
  
  i = lhint;
  if (queue[(MAX+i+1) % (MAX+2)]->val == LN) {
    /* we can push */
    queue[i]->val = val;
    lhint = (lhint+MAX+1) % (MAX+2);
    result = 1;
  } else {
    /* full: cannot push */
    result = 0; 
  }
  return result;
}	


int rnull(val_t val) {
  return (val == RN);
}


int lnull(val_t val) {
  return (val == LN);
}


int deque_rightpush_parsingoracle(circarr_t *queue, val_t val, int transactional) {
  int result = 0;
  
#ifdef SEQUENTIAL
  int i = -1;
  
  /* search leftmost RN */
  while (i < 0) {
    for (i = MAX+1; i >= 0; i--) {
      if (queue[i]->val == RN && queue[(MAX+i+1) % (MAX+2)]->val != RN) {
	break;
      }
    }
  }
  if (queue[(i+1) % (MAX+2)]->val == RN) {
    /* we can push */
    queue[i]->val = val;
    result = 1;
  } else {
    /* full: cannot push */
    result = 0; 
  }
  
#elif defined STM			
  node_t *new,  *prev, *cur;
  int i; 
  
  TX_START(EL);
  
  /* search leftmost RN */
  do {
    i=-1;
    while (i < 0) {
      for (i = MAX+1; i >= 0; i--) {
	prev = queue[(MAX+i+1) % (MAX+2)];
	cur = queue[i];
	if (rnull(cur->val) && !rnull(prev->val))
	  break;
      }
    }
    prev = (node_t *)TX_LOAD(&queue[(MAX+i+1) % (MAX+2)]);
    cur = (node_t *)TX_LOAD(&queue[i]);
  } while(!rnull(cur->val) || rnull(prev->val));
  
  if (rnull((val_t)TX_LOAD(&queue[(i+1) % (MAX+2)]->val))) {
    new = (node_t *)MALLOC(sizeof(node_t));
    /* we can push */
    new->val = val;
    new->ctr = 0;
    TX_STORE(&queue[i], new);
    FREE(queue[i], sizeof(node_t *));
    result = 1;
  } else if (lnull((val_t)TX_LOAD(&queue[(i+1) % (MAX+2)]->val))) {
    /* full: cannot push */
    result = 0; 
  } 
  TX_END;
  
#elif defined LOCKFREE			
  result = herlihy_rightpush_hint(queue, val, transactional);
#endif	
  
  return result;
}


int deque_rightpush(circarr_t *queue, val_t val, int transactional) {
  int result = 0;
  
#ifdef SEQUENTIAL
  int i = -1;
  
  /* search leftmost RN */
  i = rhint;
  if (queue[(i+1) % (MAX+2)]->val == RN) {
    /* we can push */
    queue[i]->val = val;
    /* increment leftmost RN index */
    rhint = (rhint+1) % (MAX+2);
    result = 1;
  } else {
    /* full: cannot push */
    result = 0; 
  }
  
#elif defined STM			
  node_t *new;
  int i; 
  
  if (transactional > 1) {

    TX_START(EL);
    /* search leftmost RN */
    i = (val_t)TX_LOAD(&rhint);
    
    if (rnull((val_t)TX_LOAD(&queue[(i+1) % (MAX+2)]->val))) {
      /* we can push */
      new = (node_t *)MALLOC(sizeof(node_t));
      new->val = val;
      new->ctr = 0;
      TX_STORE(&queue[i], new);
      /* increment the leftmost RN index */
      TX_STORE(&rhint, (i+1) % (MAX+2));
      FREE(queue[i], sizeof(node_t *));
      result = 1;
    } else if (lnull((val_t)TX_LOAD(&queue[(i+1) % (MAX+2)]->val))) {
      /* full: cannot push */
      result = 0; 
    } 
    TX_END;

  } else {

    TX_START(NL);
    /* search leftmost RN */
    i = (val_t)TX_LOAD(&rhint);
    
    if (rnull((val_t)TX_LOAD(&queue[(i+1) % (MAX+2)]->val))) {
      /* we can push */
      new = (node_t *)MALLOC(sizeof(node_t));
      new->val = val;
      new->ctr = 0;
      TX_STORE(&queue[i], new);
      /* increment the leftmost RN index */
      TX_STORE(&rhint, (i+1) % (MAX+2));
      FREE(queue[i], sizeof(node_t *));
      result = 1;
    } else if (lnull((val_t)TX_LOAD(&queue[(i+1) % (MAX+2)]->val))) {
      /* full: cannot push */
      result = 0; 
    } 
    TX_END;
  
  }
    
#elif defined LOCKFREE			
  result = herlihy_rightpush_hint(queue, val, transactional);
#endif	
  
  return result;
}


int deque_leftpush_parsingoracle(circarr_t *queue, val_t val, int transactional) {
  int result = 0;
  
#ifdef SEQUENTIAL
  int i = -1;
  
  /* search rightmost LN */
  while (i < 0) {
    for (i = 0; i <= MAX+1; i++) {
      if (queue[i]->val == LN && queue[(i+1) % (MAX+2)]->val != LN) {
	break;
      }
    }
  }
  if (queue[(MAX+i+1) % (MAX+2)]->val == LN) {
    /* we can push */
    queue[i]->val = val;
    result = 1;
  } else {
    /* full: cannot push */
    result = 0; 
  }
  
#elif defined STM			

  node_t *new, *prev, *cur;
  int i; 
  
  if (transactional > 2) {
  
    TX_START(EL);	
    /* search rightmost LN */
    do {
      i=-1;
      while (i < 0 || i > MAX+1) {
	for (i = 0; i <= MAX+1; i++) {
	  prev = queue[(i+1) % (MAX+2)];
	  cur = queue[i];
	  if (lnull(cur->val) && !lnull(prev->val))
	    break;
	}
      }
      prev = (node_t*)TX_LOAD(&queue[(i+1) % (MAX+2)]);
      cur = (node_t*)TX_LOAD(&queue[i]);
    } while (!lnull(cur->val) || lnull(prev->val));
    
    if (lnull(TX_LOAD(&queue[(MAX+i+1) % (MAX+2)]->val))) {
      /* we can push */
      new = (node_t *)MALLOC(sizeof(node_t));
      new->val = val;
      new->ctr = 0;
      TX_STORE(&queue[i], new);
      FREE(queue[i], sizeof(node_t *));
      result = 1;
    } 
    TX_END;
   
  } else {
 
    TX_START(NL);	
    /* search rightmost LN */
    do {
      i=-1;
      while (i < 0 || i > MAX+1) {
	for (i = 0; i <= MAX+1; i++) {
	  prev = queue[(i+1) % (MAX+2)];
	  cur = queue[i];
	  if (lnull(cur->val) && !lnull(prev->val))
	    break;
	}
      }
      prev = (node_t*)TX_LOAD(&queue[(i+1) % (MAX+2)]);
      cur = (node_t*)TX_LOAD(&queue[i]);
    } while (!lnull(cur->val) || lnull(prev->val));
    
    if (lnull(TX_LOAD(&queue[(MAX+i+1) % (MAX+2)]->val))) {
      /* we can push */
      new = (node_t *)MALLOC(sizeof(node_t));
      new->val = val;
      new->ctr = 0;
      TX_STORE(&queue[i], new);
      FREE(queue[i], sizeof(node_t *));
      result = 1;
    } 
    TX_END;
   
  }

#elif defined LOCKFREE			
  result = herlihy_leftpush_hint(queue, val, transactional);
#endif	
  
  return result;
}


int deque_leftpush(circarr_t *queue, val_t val, int transactional) {
  int result = 0;
  
#ifdef SEQUENTIAL
  int i;
  
  /* search rightmost LN */
  i = lhint;
  if (queue[(MAX+i+1) % (MAX+2)]->val == LN) {
    /* we can push */
    queue[i]->val = val;
    lhint = (lhint+MAX+1) % (MAX+2);
    result = 1;
  } else {
    /* full: cannot push */
    result = 0; 
  }
  
#elif defined STM			
  node_t *new; 
  int i; 
  
  TX_START(EL);		
  /* search rightmost LN */
  i = (val_t)TX_LOAD(&lhint);
  
  if (lnull(TX_LOAD(&queue[(MAX+i+1) % (MAX+2)]->val))) {
    /* we can push */
    new = (node_t *)MALLOC(sizeof(node_t));
    new->val = val;
    new->ctr = 0;
    TX_STORE(&queue[i], new);
    TX_STORE(&lhint, (i+MAX+1) % (MAX+2));
    FREE(queue[i], sizeof(node_t *));
    result = 1;
  } 
  TX_END;
  
#elif defined LOCKFREE			
  result = herlihy_leftpush_hint(queue, val, transactional);
#endif	
  
  return result;
}


int deque_rightpop_parsingoracle(circarr_t *queue, int transactional) {
  int result = 0;
  
#ifdef SEQUENTIAL
  int i = -1;
  
  /* search leftmost RN */
  while (i < 0) {
    for (i = MAX+1; i >= 0; i--) {
      if (queue[i]->val == RN && queue[(MAX+i+1) % (MAX+2)]->val != RN) {
	break;
      }
    }
  }
  if (queue[(MAX+i+1) % (MAX+2)]->val == LN) { 
    result = 0; // empty
  } else {
    queue[(MAX+i+1) % (MAX+2)]->val = RN;
    result = 1;
  }
  
#elif defined STM
  node_t *new; 
  val_t cur_val, prev_val;
  int i; 
  
  TX_START(EL);	
  /* search leftmost RN */
  do {
    i=-1;
    while (i < 0) {
      for (i = MAX+1; i >= 0; i--) {
	cur_val = queue[i]->val;
	prev_val = queue[(MAX+i+1) % (MAX+2)]->val;
	if (rnull(cur_val) && !rnull(prev_val))
	  break;
      }
    }
    cur_val = (val_t)TX_LOAD(&queue[i]->val);
    prev_val = (val_t)TX_LOAD(&queue[(MAX+i+1) % (MAX+2)]->val);
  } while(!rnull(cur_val) || rnull(prev_val));
  
  if (!lnull((val_t)TX_LOAD(&queue[(MAX+i+1) % (MAX+2)]->val))) {
    /* we can pop */
    new = (node_t *)MALLOC(sizeof(node_t));
    new->val = RN;
    new->ctr = 0;
    TX_STORE(&queue[(MAX+i+1) % (MAX+2)], new);
    FREE(queue[(MAX+i+1) % (MAX+2)], sizeof(node_t *));
    result = 1;
  }
  TX_END;
  
#elif defined LOCKFREE			
  result = herlihy_rightpop_hint(queue, transactional);
#endif	

  return result;
}


int deque_rightpop(circarr_t *queue, int transactional) {
  int result = 0;
  
#ifdef SEQUENTIAL
  int i = -1;
  
  /* search leftmost RN */
  i = rhint;
  if (queue[(MAX+i+1) % (MAX+2)]->val == LN) { 
    result = 0; // empty
  } else {
    queue[(MAX+i+1) % (MAX+2)]->val = RN;
    rhint = (rhint+MAX+1) % (MAX+2);
    result = 1;
  }
  
#elif defined STM
  node_t *new, *prev; 
  int i;
  
  TX_START(EL);	
  /* search leftmost RN */
  i = (val_t)TX_LOAD(&rhint);
  
  prev = (node_t *)TX_LOAD(&queue[(MAX+i+1) % (MAX+2)]);
  if (!lnull((val_t)prev->val)) {
    /* we can pop */
    new = (node_t *)MALLOC(sizeof(node_t));
    new->val = RN;
    new->ctr = 0;
    TX_STORE(&queue[(MAX+i+1) % (MAX+2)], new);
    TX_STORE(&rhint, (i+MAX+1) % (MAX+2));
    FREE(queue[(MAX+i+1) % (MAX+2)], sizeof(node_t *));
    result = 1;
  } 
  TX_END;
  
#elif defined LOCKFREE			
  result = herlihy_rightpop_hint(queue, transactional);
#endif	

  return result;
}


int deque_leftpop_parsingoracle(circarr_t *queue, int transactional) {
  int result = 0;
  
#ifdef SEQUENTIAL
  int i = -1;
  
  while (i < 0 || i > MAX+1) {
    for (i = 0; i <= MAX+1; i++) {
      if (queue[i]->val == LN && queue[(i+1) % (MAX+2)]->val != LN) {	
	break;
      }
    }
  }
  if (queue[(i+1) % (MAX+2)]->val == RN) { 
    result = 0; // empty
  } else {
    queue[(i+1) % (MAX+2)]->val = LN;
    result = 1;
  }
  
#elif defined STM
  node_t *new, *prev;
  val_t cur_val, prev_val = 0;
  int i;
  
  TX_START(EL);
  /* search rightmost LN */
  do {
    i=-1;
    while (i < 0 || i > MAX+1) {
      for (i = 0; i <= MAX+1; i++) {
	cur_val = queue[i]->val; 
	prev_val = queue[(i+1) % (MAX+2)]->val;
	if (lnull(cur_val) && !lnull(prev_val))
	  break;
      }
    }
    cur_val = (val_t)TX_LOAD(&queue[i]->val);
    prev_val = (val_t)TX_LOAD(&queue[(i+1) % (MAX+2)]->val);
  } while (!lnull(cur_val) || lnull(prev_val));
  
  prev = (node_t*)TX_LOAD(&queue[(i+1) % (MAX+2)]->val);
  if (!rnull(prev->val)) { 
    /* we can pop */
    new = (node_t *)MALLOC(sizeof(node_t));
    new->val = LN;
    new->ctr = 0;
    TX_STORE(&queue[(i+1) % (MAX+2)], new);
    FREE(queue[(i+1) % (MAX+2)], sizeof(node_t *));
    result = 1;
  }
  TX_END;
  
#elif defined LOCKFREE			
  result = herlihy_leftpop_hint(queue, transactional);
#endif	
  
  return result;
}


int deque_leftpop(circarr_t *queue, int transactional) {
  int result = 0;
  
#ifdef SEQUENTIAL
  int i = -1;
  
  i = lhint;
  if (queue[(i+1) % (MAX+2)]->val == RN) { 
    result = 0; // empty
  } else {
    queue[(i+1) % (MAX+2)]->val = LN;
    lhint = (lhint+1) % (MAX+2);
    result = 1;
  }
  
#elif defined STM
  node_t *new, *prev; 
  int i; 
  
  TX_START(EL);
  /* search rightmost LN */
  i = (val_t)TX_LOAD(&lhint);
  
  prev = (node_t*)TX_LOAD(&queue[(i+1) % (MAX+2)]);
  if (!rnull(prev->val)) { 
    /* we can pop */
    new = (node_t *)MALLOC(sizeof(node_t));
    new->val = LN;
    new->ctr = 0;
    TX_STORE(&queue[(i+1) % (MAX+2)], new);
    TX_STORE(&lhint, (i+1) % (MAX+2));
    FREE(queue[(i+1) % (MAX+2)], sizeof(node_t *));
    result = 1;
  }
  TX_END;
  
#elif defined LOCKFREE			
  result = herlihy_leftpop_hint(queue, transactional);
#endif	

  return result;
}

