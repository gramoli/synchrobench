/*
 *  intset.c
 *  
 *  Pop and push operations 
 *  that call stm-based / lock-free counterparts.
 *
 *  Created by Vincent Gramoli on 1/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "herlihy.h"

int deque_rightpush(circarr_t *queue, val_t val, int transactional);
int deque_leftpush(circarr_t *queue, val_t val, int transactional);

int deque_leftpop(circarr_t *queue, int transactional);
int deque_rightpop(circarr_t *queue, int transactional);

int seq_rightpush(circarr_t *queue, val_t val, int transactional);
int seq_leftpush(circarr_t *queue, val_t val, int transactional);


