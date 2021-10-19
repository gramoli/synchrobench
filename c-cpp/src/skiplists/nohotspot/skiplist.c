/*
 * skiplist.c: definitions of the skip list data stucture
 * 
 * Author: Ian Dick, 2013
 *
 */

/*

Module overview

This module provides the basic structures used to create the 
skip list data structure, as described in:
Crain, T., Gramoli, V., Raynal, M. (2013) 
"No Hot-Spot Non-Blocking Skip List", to appear in
The 33rd IEEE International Conference on Distributed Computing 
Systems (ICDCS).

One approach to designing a skip list data structure is to have
one kind of node, and to assign each node in the list a level.
A node with level l has l backwards pointers and l forward pointers,
one for each level of the skip list. Skip lists designed this
way have index level nodes that are implicit, are are handled by
multiple pointers per node.

The approach taken here is to explicitly create the index nodes and
have these as separate structures to regular nodes. The reason for
this is that the background maintenance method used here is much easier
to implement this way.

*/

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "skiplist.h"
#include "background.h"
#include "garbagecoll.h"
#include "ptst.h"

static int gc_id[NUM_LEVELS];

/* - Public skiplist interface - */

/**
 * node_new - create a new bottom-level node
 * @key: the key for the new node
 * @val: the val for the new node
 * @prev: the prev node pointer for the new node
 * @next: the next node pointer for the new node
 * @level: the level for the new node
 * @ptst: the per-thread state
 *
 * Note: All nodes are originally created with
 * marker set to 0 (i.e. they are unmarked).
 */
node_t* node_new(sl_key_t key, val_t val, node_t *prev, node_t *next,
                            unsigned int level, ptst_t *ptst)
{
        node_t *node;

        node = gc_alloc(ptst, gc_id[NODE_LEVEL]);

        node->key       = key;
        node->val       = val;
        node->prev      = prev;
        node->next      = next;
        node->level     = level;
        node->marker    = 0;

        assert (node->next != node);

        return node;
}

/**
 * inode_new - create a new index node
 * @right: the right inode pointer for the new inode
 * @down: the down inode pointer for the new inode
 * @node: the node pointer for the new inode
 * @ptst: per-thread state
 */
inode_t* inode_new(inode_t *right, inode_t *down, node_t *node, ptst_t *ptst)
{
        inode_t *inode;

        inode = gc_alloc(ptst, gc_id[INODE_LEVEL]);

        inode->right = right;
        inode->down = down;
        inode->node = node;

        return inode;
}

/**
 * node_delete - delete a bottom-level node
 * @node: the node to delete
 */
void node_delete(node_t *node, ptst_t *ptst)
{
        gc_free(ptst, (void*)node, gc_id[NODE_LEVEL]);
}

/**
 * inode_delete - delete an index node
 * @inode: the index node to delete
 */
void inode_delete(inode_t *inode, ptst_t *ptst)
{
        gc_free(ptst, (void*)inode, gc_id[INODE_LEVEL]);
}

/**
 * set_new - create a new set implemented as a skip list
 * @bg_start: if 1 start the bg thread, otherwise don't
 *
 * Returns a newly created skip list set.
 * Note: A background thread to update the index levels of the
 * skip list is created and kick-started as part of this routine.
 */
set_t* set_new(int start)
{
        set_t *set;

        set = malloc(sizeof(set_t));
        if (!set) {
                perror("Failed to malloc a set\n");
                exit(1);
        }

        set->head = malloc(sizeof(node_t));
        set->head->key    = 0;
        set->head->val    = NULL;
        set->head->prev   = NULL;
        set->head->next   = NULL;
        set->head->level  = 1;
        set->head->marker = 0;

        set->top = malloc(sizeof(inode_t));
        set->top->right = NULL;
        set->top->down  = NULL;
        set->top->node  = set->head;

        set->raises = 0;

        bg_init(set);
        if (start)
                bg_start(0);

        return set;
}

/**
 * set_delete - delete the set
 * @set: the set to delete
 */
void set_delete(set_t *set)
{
        inode_t *top;
        inode_t *icurr;
        inode_t *inext;
        node_t  *curr;
        node_t  *next;

        /* stop the background thread */
        bg_stop();

        /* warning - we are not deallocating the memory for the skip list */
}

/**
 * set_print - print the set
 * @set: the skip list set to print
 * @flag: if non-zero include logically deleted nodes in the count
 */
void set_print(set_t *set, int flag)
{
        node_t  *node   = set->head;
        inode_t *ihead  = set->top;
        inode_t *itemp  = set->top;

        /* print the index items */
        while (NULL != ihead) {
                while (NULL != itemp) {
                        printf("%lu ", itemp->node->key);
                        itemp = itemp->right;
                }
                printf("\n");
                ihead = ihead->down;
                itemp = ihead;
        }

        while (NULL != node) {
                if (flag && (NULL != node->val && node->val != node))
                        printf("%lu ", node->key);
                else if (!flag)
                        printf("%lu ", node->key);
                node = node->next;
        }
        printf("\n");
}

/**
 * set_size - print the size of the set
 * @set: the set to print the size of
 * @flag: if non-zero include logically deleted nodes in the count
 *
 * Return the size of the set.
 */
int set_size(set_t *set, int flag)
{
        struct sl_node *node = set->head;
        int size = 0;

        node = node->next;
        while (NULL != node) {
                if (flag && (NULL != node->val && node != node->val))
                        ++size;
                else if (!flag)
                        ++size;
                node = node->next;
        }

        return size;
}

/**
 * set_subsystem_init - initialise the set subsystem
 */
void set_subsystem_init(void)
{
        gc_id[NODE_LEVEL]  = gc_add_allocator(sizeof(node_t));
        gc_id[INODE_LEVEL] = gc_add_allocator(sizeof(inode_t));
}
