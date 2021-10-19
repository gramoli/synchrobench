#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_
#include <stdbool.h>

/**
 * Copyright 2014 Maya Arbel (mayaarl [at] cs [dot] technion [dot] ac [dot] il).
 * 
 * This file is part of Citrus. 
 * 
 * Citrus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Author Maya Arbel
 */


#define infinity 2147483647 


typedef struct node_t {
  int key;
  struct node_t* child[2];
  pthread_mutex_t lock;
  bool marked;
  int tag[2];
  int value;
  } node_t;

typedef struct node_t* node;


node init();
int contains(node root, int key);
bool insert(node root, int key, int value);
bool delete(node root, int key);

#endif
