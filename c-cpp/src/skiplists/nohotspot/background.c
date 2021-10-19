/*
 * background.c: background index-level skip list maintenance
 *
 * Author: Ian Dick, 2013.
 *
 */

/*

Module Overview

This module provides the routines necessary for skip list
maintenance to be performed in the background by a single
maintenance thread, as in the algorithm specified here:
Crain, T., Gramoli, V., Raynal, M. (2013) 
"No Hot-Spot Non-Blocking Skip List", to appear in
The 33rd IEEE International Conference on Distributed Computing 
Systems (ICDCS).

Maintenance to be performed in the background includes all
modifications to the index levels of the skip list, as well as
completion of deletions from the node level that are logically
complete but not physically complete (i.e. logically deleted 
nodes that are still reachable).

The idea behind having a background thread carry out these operations
as opposed to allowing all worker threads to modify index items 
during operation is that we avoid contention on the upper index levels.
If a skip list is subject to many update operations by many competing 
threads, then the upper index levels become contention hot spots, 
and as threads contend over these updates they
are effectively serailised due to their interference with one
another. Having a single thread adjust the index levels completely
removes the issue of index-level contention.

The background thread performs its duties by continuously traversing
the skip list and performing adjustments along the way. Since
index modifications are not multi-threaded, the requirement that
the number of index nodes at level i is approx n*(2^-i) 
(where n is the number of nodes in the skip list) can be
enforced deterministically, rather than probabilistically as is
common with other multi-threaded skip list implementations. 
However, even though balancing is deterministic this does not mean
there are concrete guarantees about how balanced the list will be at a
certain point in time. The balancing of the skip list is influenced
by how often the background thread is running and the operations
being carried out by other worker threads.

The background can be set to run only intermittently in order to
reduce the negative impact on performance the background thread can
introduce. If the background thread is constantly running then this
can interrupt other threads trying to traverse the list, as the background
thread may cause cache invalidations in other threads and cause costly
reads from memory to occur.

*/

#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "background.h"
#include "skiplist.h"
#include "garbagecoll.h"
#include "ptst.h"
#include "common.h"

/* - Private variables - */

static set_t *set;	        /* the set to maintain */
static pthread_t bg_thread;	/* background thread */

/* Uncomment to collect background stats - reduces performance */
/* #define BG_STATS */

typedef struct bg_stats bg_stats_t;
static struct bg_stats {
        int raises;
        int loops;
        int lowers;
        int delete_succeeds;
} bg_stats;

/* to keep track of background state */
static int bg_finished;
static int bg_running;

/* for deciding whether to lower the skip list index level */
static int bg_non_deleted;
static int bg_tall_deleted;

/* the amount of time the bg thread sleeps for each iteration */
static int bg_sleep_time;

/* - Private Functions - */

static void* bg_loop(void *args);
static void bg_trav_nodes(ptst_t *ptst);
static void bg_lower_ilevel(inode_t *new_low, ptst_t *ptst);
static int bg_raise_nlevel(inode_t *inode, ptst_t *ptst);
static int bg_raise_ilevel(inode_t *iprev,inode_t *iprev_tall,
                           int height, ptst_t *ptst);

/**
 * bg_loop - loop for maintaining index levels
 * @args: void* args as per pthread_create requirements
 *
 * Returns a void* value as per pthread_create requirements.
 * Note: Do this loop forever while the program is running.
 */
static void* bg_loop(void *args)
{
        inode_t *inode;
        inode_t *inew;
        inode_t *inodes[MAX_LEVELS];
        int raised = 0; /* keep track of if we raised index level */
        int threshold;  /* for testing if we should lower index level */
        int i;
        struct sl_ptst *ptst;

        assert(NULL != set);

        while (1) {
                if (bg_finished)
                        break;

                usleep(bg_sleep_time);

                #ifdef USE_GC
                ptst = ptst_critical_enter();
                #endif

                for (i = 0; i < MAX_LEVELS; i++)
                        inodes[i] = NULL;

                #ifdef BG_STATS
                ++bg_stats.loops;
                #endif

                bg_non_deleted = 0;
                bg_tall_deleted = 0;

                /* traverse the node level and do physical deletes */
                bg_trav_nodes(ptst);

                assert(set->head->level < MAX_LEVELS);

                /* get the first index node at each level */
                inode = set->top;
                for (i = set->head->level - 1; i >= 0; i--) {
                        inodes[i] = inode;
                        assert(NULL != inodes[i]);
                        inode = inode->down;
                }
                assert(NULL == inode);

                /* raise bottom level nodes */
                raised = bg_raise_nlevel(inodes[0], ptst);

                if (raised && (1 == set->head->level)) {
                        /* add a new index level */
                        inew = inode_new(NULL, set->top, set->head, ptst);
                        set->top = inew;
                        ++set->head->level;
                        assert(NULL == inodes[1]);
                        inodes[1] = set->top;

                        #ifdef BG_STATS
                        ++bg_stats.raises;
                        #endif
                }

                /* raise the index level nodes */
                for (i = 0; i < (set->head->level - 1); i++) {
                        assert(i < MAX_LEVELS-1);
                        raised = bg_raise_ilevel(inodes[i],/* level raised */
                                                 inodes[i + 1],/* level above */
                                                 i + 1,/* current height */
                                                 ptst);
                }

                if (raised) {
                        /* add a new index level */
                        inew = inode_new(NULL, set->top, set->head, ptst);
                        set->top = inew;
                        ++set->head->level;

                        #ifdef BG_STATS
                        ++bg_stats.raises;
                        #endif
                }

                /* if needed, remove the lowest index level */
                threshold = bg_non_deleted * 10;
                if (bg_tall_deleted > threshold) {
                        if (NULL != inodes[1]) {
                                bg_lower_ilevel(inodes[1],/* level above */
                                                ptst);

                                #ifdef BG_STATS
                                ++bg_stats.lowers;
                                #endif
                        }
                }

                #ifdef USE_GC
                ptst_critical_exit(ptst);
                #endif
        }

        return NULL;
}

/**
 * bg_trav_nodes - traverse node level of skip list and maintain
 * @ptst: per-thread state
 * 
 * Note: this will try to remove each of the nodes in the list,
 * in order to extract nodes that have already been logically deleted
 * but that are still accessible.
 */
static void bg_trav_nodes(ptst_t *ptst)
{
        node_t *prev, *node;

        assert(NULL != set && NULL != set->head);

        prev = set->head;
        node = prev->next;
        while (NULL != node) {
                bg_remove(prev, node, ptst);
                if (NULL != node->val && node != node->val)
                        ++bg_non_deleted;
                else if (node->level >= 1)
                        ++bg_tall_deleted;
                prev = node;
                node = node->next;
        }
}

/**
 * bg_raise_nlevel - raise level 0 nodes into index levels 
 * @inode: the index node at the start of the bottom index level
 * @ptst: per-thread state
 *
 * Returns 1 if a node was raised and 0 otherwise.
 */
static int bg_raise_nlevel(inode_t *inode, ptst_t *ptst)
{
        int raised = 0;
        node_t *prev, *node, *next;
        inode_t *inew, *above, *above_prev;

        above = above_prev = inode;

        assert(NULL != inode);

        prev = set->head;
        node = set->head->next;

        if (NULL == node)
                return 0;

        next = node->next;

        while (NULL != next) {
                /* don't raise deleted nodes */
                if (node != node->val) {
                        if (((prev->level == 0) &&
                             (node->level == 0)) &&
                             (next->level == 0)) {

                                raised = 1;

                                /* get the correct index above and behind */
                                while (above && above->node->key < node->key) {
                                        above = above->right;
                                        if (above != inode->right)
                                                above_prev = above_prev->right;
                                }


                                /* add a new index item above node */
                                inew = inode_new(above_prev->right, NULL,
                                                 node, ptst);
                                above_prev->right = inew;
                                node->level = 1;
                                above_prev = inode = above = inew;
                        }
                }
                prev = node;
                node = next;
                next = next->next;
        }

        return raised;
}

/**
 * bg_raise_ilevel - raise the index levels
 * @iprev: the first index node at this level
 * @iprev_tall: the first index node at the next highest level
 * @height: the height of the level we are raising
 * @ptst: per-thread state
 *
 * Returns 1 if a node was raised and 0 otherwise.
 */
static int bg_raise_ilevel(inode_t *iprev, inode_t *iprev_tall,
                           int height, ptst_t *ptst)
{
        int raised = 0;
        inode_t *index, *inext, *inew, *above, *above_prev;

        above = above_prev = iprev_tall;

        assert(NULL != iprev);
        assert(NULL != iprev_tall);

        index = iprev->right;

        while ((NULL != index) && (NULL != (inext = index->right))) {
                while (index->node->val == index->node) {
                        /* skip deleted nodes */
                        iprev->right = inext;
                        if (NULL == inext)
                                break;

                        index = inext;
                        inext = inext->right;
                }
                if (NULL == inext)
                        break;
                if (((iprev->node->level <= height) &&
                     (index->node->level <= height)) &&
                     (inext->node->level <= height)) {

                        raised = 1;

                        /* get the correct index above and behind */
                        while (above && above->node->key < index->node->key) {
                                above = above->right;
                                if (above != iprev_tall->right)
                                        above_prev = above_prev->right;
                        }

                        inew = inode_new(above_prev->right, index,
                                         index->node, ptst);
                        above_prev->right = inew;
                        index->node->level = height + 1;
                        above_prev = above = iprev_tall = inew;
                }
                iprev = index;
                index = inext;
        }

        return raised;
}

/**
 * bg_lower_ilevel - lower the index level
 * @new_low: the first index item in the second lowest level
 * @ptst: per-thread state
 *
 * Note: the lowest index level is removed by nullifying
 * the reference to the lowest level from the second lowest level.
 */
void bg_lower_ilevel(inode_t *new_low, ptst_t *ptst)
{
        inode_t *old_low = new_low->down;

        /* remove the lowest index level */
        while (NULL != new_low) {
                new_low->down = NULL;
                --new_low->node->level;
                new_low = new_low->right;
        }

        /* garbage collect the old low level */
        while (NULL != old_low) {
                inode_delete(old_low, ptst);
                old_low = old_low->right;
        }
}

/* - Public Background Interface - */

/**
 * bg_init - initialise the background module
 * @s: the set to maintain
 */
void bg_init(set_t *s)
{
        set = s;
        bg_finished = 0;
        bg_running = 0;

        bg_stats.loops = 0;
        bg_stats.raises = 0;
        bg_stats.lowers = 0;
        bg_stats.delete_succeeds = 0;
}

/**
 * bg_start - start the background thread
 * @sleep_time: the time to sleep the bg thread per iteration
 *
 * Note: Only starts the background thread if it is not currently
 * running.
 */
void bg_start(int sleep_time)
{
        if (!bg_running) {
                bg_running = 1;
                bg_finished = 0;
                bg_sleep_time = sleep_time;
                pthread_create(&bg_thread, NULL, bg_loop, NULL);
        }
}

/**
 * bg_stop - stop the background thread
 */
void bg_stop(void)
{
        if (bg_running) {
                bg_finished = 1;
                pthread_join(bg_thread, NULL);
                BARRIER();
                bg_running = 0;
        }
}

/**
 * bg_print_stats - print background statistics
 *
 * Note: this is a noop if BG_STATS is not defined.
 */
void bg_print_stats(void)
{
        #ifdef BG_STATS
        printf("Loops = %i\n", bg_stats.loops);
        printf("Raises = %i\n", bg_stats.raises);
        printf("Levels = %i\n", set->head->level);
        printf("Lowers = %i\n", bg_stats.lowers);
        printf("Delete Succeeds = %i\n", bg_stats.delete_succeeds);
        #endif
}

/**
 * bg_help_remove - finish physically removing a node
 * @prev: the node before the one to remove
 * @node: the node to finish removing
 * @ptst: per-thread state
 *
 * Note: This operation will only be carried out if @node
 * has been successfully marked for deletion (i.e. its value points
 * to itself, and the node must now be deleted). First we insert a marker
 * node directly after @node. Then, if no nodes have been inserted in
 * between @prev and @node, physically remove @node and the marker
 * by pointing @prev->next past these nodes.
 */
void bg_help_remove(node_t *prev, node_t *node, ptst_t *ptst)
{
        node_t *n, *new;

        #ifdef BG_STATS 
        int retval;
        #endif

        assert(NULL != prev);
        assert(NULL != node);

        if (node->val != node || node->marker)
                return;

        n = node->next;
        while (NULL == n || !n->marker) {
                        new = node_new(0, NULL, node, n, 0, ptst);
                        new->val = new;
                        new->marker = 1;
                        CAS(&node->next, n, new);

                        assert (node->next != node);

                        n = node->next;
        }

        if (prev->next != node || prev->marker)
                return;

        /* remove the nodes */
        #ifdef BG_STATS
        retval = CAS(&prev->next, node, n->next);
        #else
        CAS(&prev->next, node, n->next);
        #endif

        assert (prev->next != prev);

        #ifdef BG_STATS
        if (retval)
                ++bg_stats.delete_succeeds;
        #endif
}

/**
 * bg_remove - start the physical removal of @node
 * @prev: the node before the one to remove
 * @node: the node to remove
 * @ptst: per-thread state
 *
 * Note: we only remove nodes that are of height 0 (i.e. they
 * don't have index nodes above). Nodes with index items are 
 * removed a different way, using index height changes.
 */
void bg_remove(node_t *prev, node_t *node, ptst_t *ptst)
{
        assert(NULL != node);

        if (0 == node->level) {
                /* only remove short nodes */
                CAS(&node->val, NULL, node);
                if (node->val == node)
                        bg_help_remove(prev, node, ptst);
        }
}
