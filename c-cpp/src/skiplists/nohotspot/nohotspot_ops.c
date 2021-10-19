/*
 * nohotspot_ops.c: contains/insert/delete skip list operations
 *
 * Author: Ian Dick, 2013
 *
 */

/*

Module Overview

This module provides the three basic skip list operations:
> insert(key, val)
> contains(key)
> delete(key)

These abstract operations are implemented using the algorithms
described in:
Crain, T., Gramoli, V., Raynal, M. (2013) 
"No Hot-Spot Non-Blocking Skip List", to appear in
The 33rd IEEE International Conference on Distributed Computing 
Systems (ICDCS).

This multi-thread skip list implementation is non-blocking, 
and uses the Compare-and-swap hardware primitive to manage 
concurrency and thread synchronisation. As a result,
the three public operations described in this module require
rather complicated implementations, in order to ensure that 
the data structure can be used correctly.

All the abstract operations first use the private function
sl_do_operation() to retrieve two nodes from the skip list:
node - the node with greatest key k such that k is less than
       or equal to the search key
next - the node with the lowest key k such that k is greater than
       the search key
After node and next have been retrieved using sl_do_operation(),
the appropriate action is taken based on the abstract operation
being performed. For example, if we are conducting a delete and
node.key is equal to the search key, then we try to extract node.

Worthy to note is that worker threads in this skip list implementation
do not perform maintenance of the index levels: this is carried 
out by a background thread. Worker threads do, however, perform
maintenance on the node level. During sl_do_operation(), if a worker
thread notices that a node it is traversing is logically deleted,
it will stop what it is doing and attempt to finish deleting the
node, and then resume it's previous course of action once this is
finished.

*/

#include <stdlib.h>
#include <assert.h>
#include <atomic_ops.h>

#include "common.h"
#include "skiplist.h"
#include "nohotspot_ops.h"
#include "background.h"
#include "garbagecoll.h"
#include "ptst.h"

/* - Private Functions - */

static int sl_finish_contains(sl_key_t key, node_t *node, val_t node_val,
                              ptst_t *ptst);
static int sl_finish_delete(sl_key_t key, node_t *node, val_t node_val,
                            ptst_t *ptst);
static int sl_finish_insert(sl_key_t key, val_t val, node_t *node,
                            val_t node_val, node_t *next, ptst_t *ptst);

/**
 * sl_finish_contains - contains skip list operation
 * @key: the search key
 * @node: the left node from sl_do_operation()
 * @node_val: @node value
 * @ptst: per-thread state
 *
 * Returns 1 if the search key is present and 0 otherwise.
 */
static int sl_finish_contains(sl_key_t key, node_t *node, val_t node_val,
                              ptst_t *ptst)
{
        int result = 0;

        assert(NULL != node);

        if ((key == node->key) && (NULL != node_val))
                result = 1;

        return result;
}

/**
 * sl_finish_delete - delete skip list operation
 * @key: the search key
 * @node: the left node from sl_do_operation()
 * @node_val: @node value
 *
 * Returns 1 on success, 0 if the search key is not present,
 * and -1 if the key is present but the node is already 
 * logically deleted, or if the CAS to logically delete fails.
 */
static int sl_finish_delete(sl_key_t key, node_t *node, val_t node_val,
                            ptst_t *ptst)
{
        int result = -1;

        assert(NULL != node);

        if (node->key != key)
                result = 0;
        else {
                if (NULL != node_val) {
                        /* loop until we or someone else deletes */
                        while (1) {
                                node_val = node->val;
                                if (NULL == node_val || node == node_val) {
                                        result = 0;
                                        break;
                                }
                                else if (CAS(&node->val, node_val, NULL)) {
                                        result = 1;

                                        break;
                                }
                        }
                } else {
                        /* Already logically deleted */
                        result = 0;
                }
        }

        return result;
}

/**
 * sl_finish_insert - insert skip list operation
 * @key: the search key
 * @val: the search value
 * @node: the left node from sl_do_operation()
 * @node_val: @node value
 * @next: the right node from sl_do_operation()
 *
 * Returns:
 * > 1 if @key is present in the set and the corresponding node
 *   is logically deleted and the undeletion operation succeeds.
 * > 1 if @key is not present in the set and insertion operation
 *   succeeds.
 * > 0 if @key is present in the set and not null.
 * > -1 if @key is present in the set and value of corresponding
 *   node is not null and logical un-deletion fails due to concurrency.
 * > -1 if @key is not present in the set and insertion operation
 *   fails due to concurrency.
 */
static int sl_finish_insert(sl_key_t key, val_t val, node_t *node,
                            val_t node_val, node_t *next, ptst_t *ptst)
{
        int result = -1;
        node_t *new;

        if (node->key == key) {
                if (NULL == node_val) {
                        if (CAS(&node->val, node_val, val))
                                result = 1;
                } else {
                        result = 0;
                }
        } else {
                new = node_new(key, val, node, next, 0, ptst);
                if (CAS(&node->next, next, new)) {

                        assert (node->next != node);

                        if (NULL != next)
                                next->prev = new; /* safe */
                        result = 1;
                } else {
                        node_delete(new, ptst);
                }
        }

        return result;
}

/* - The public nohotspot_ops interface - */

/**
 * sl_do_operation - find node and next for this operation
 * @set: the skip list set
 * @optype: the type of operation this is
 * @key: the search key
 * @val: the seach value
 *
 * Returns the result of the operation.
 * Note: @val can be NULL. 
 */
int sl_do_operation(set_t *set, sl_optype_t optype, sl_key_t key, val_t val)
{
        inode_t *item = NULL, *next_item = NULL;
        node_t *node = NULL, *next = NULL;
        val_t node_val = NULL, *next_val = NULL;
        int result = 0;
        ptst_t *ptst;

        assert(NULL != set);

#ifdef USE_GC
        ptst = ptst_critical_enter();
#endif

        /* find an entry-point to the node-level */
        item = set->top;
        while (1) {
                next_item = item->right;
                if (NULL == next_item || next_item->node->key > key) {
                        next_item = item->down;
                        if (NULL == next_item) {
                                node = item->node;
                                break;
                        }
                } else if (next_item->node->key == key) {
                        node = item->node;
                        break;
                }
                item = next_item;
        }
        /* find the correct node and next */
        while (1) {
                while (node == (node_val = node->val)) {
                        node = node->prev;
                }
                next = node->next;
                if (NULL != next) {
                        next_val = next->val;
                        if ((node_t*)next_val == next) {
                                bg_help_remove(node, next, ptst);
                                continue;
                        }
                }
                if (NULL == next || next->key > key) {
                        if (CONTAINS == optype)
                                result = sl_finish_contains(key, node, node_val,
                                                            ptst);
                        else if (DELETE == optype)
                                result = sl_finish_delete(key, node, node_val,
                                                          ptst);
                        else if (INSERT == optype)
                                result = sl_finish_insert(key, val, node,
                                                          node_val, next, ptst);
                        if (-1 != result)
                                break;
                        continue;
                }
                node = next;
        }

#ifdef USE_GC
        ptst_critical_exit(ptst);
#endif

        return result;
}
