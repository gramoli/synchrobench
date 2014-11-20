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

The maintenance to be performed in the background includes all
modifications to the index levels of the skip list, and also
completion of deletions from the node level that are logically
complete but not physically complete (i.e. logically deleted
nodes are still reachable).

The idea behind having a background thread carry out these operations
as opposed to allowing all worker threads to modify index items
during operation is that we avoid contention on the upper index levels.
If a skip list is subject to many update operations by many competing
threads, then the upper index levels become contention hot spots,
and as threads contend for these index nodes their operations
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

*/

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>

#include "common.h"
#include "background.h"
#include "skiplist.h"
#include "garbagecoll.h"
#include "ptst.h"

static set_t *set;      /* the set to maintain */

static pthread_t bg_thread;    /* background thread */

/* Uncomment to collect background stats - reduces performance */
#define BG_STATS

static struct sl_background_stats {
        unsigned long raises;
        unsigned long loops;
        unsigned long lowers;
        unsigned long delete_attempts;
        unsigned long delete_succeeds;
        unsigned long should_delete;
} bg_stats;

/* to keep track of background state */
VOLATILE static int bg_finished;
VOLATILE static int bg_running;

/* for deciding whether to lower the skip list index level */
static int bg_non_deleted;
static int bg_deleted;
static int bg_tall_deleted;

static int bg_sleep_time;
static int bg_counter;
static int bg_go;

int bg_should_delete;

/* - Private Functions - */

static void* bg_loop(void *args);
static int bg_trav_nodes(ptst_t *ptst);
static void bg_lower_ilevel(ptst_t *ptst);
static int bg_raise_ilevel(int height, ptst_t *ptst);
static void get_index_above(node_t *head,
                            node_t **prev,
                            node_t **node,
                            unsigned long i,
                            unsigned long key,
                            unsigned long zero);

/**
 * bg_loop - loop for maintaining index levels
 * @args: void* args as per pthread_create requirements
 *
 * Returns a void* value as per pthread_create requirements.
 * Note: Do this loop forever while the program is running.
 */
static void* bg_loop(void *args)
{
        node_t  *head  = set->head;
        int raised = 0; /* keep track of if we raised index level */
        int threshold;  /* for testing if we should lower index level */
        unsigned long i;
        ptst_t *ptst = NULL;
        unsigned long zero;

        assert(NULL != set);
        bg_counter = 0;
        bg_go = 0;
        bg_should_delete = 1;
        BARRIER();

        #ifdef BG_STATS
        bg_stats.raises = 0;
        bg_stats.loops = 0;
        bg_stats.lowers = 0;
        bg_stats.delete_attempts = 0;
        bg_stats.delete_succeeds = 0;
        #endif

        while (1) {

                usleep(bg_sleep_time);

                if (bg_finished)
                        break;

                zero = sl_zero;

                #ifdef BG_STATS
                ++(bg_stats.loops);
                #endif

                bg_non_deleted = 0;
                bg_deleted = 0;
                bg_tall_deleted = 0;

                // traverse the node level and try deletes/raises
                raised = bg_trav_nodes(ptst);

                if (raised && (1 == head->level)) {
                        // add a new index level

                        // nullify BEFORE we increase the level
                        head->succs[IDX(head->level, zero)] = NULL;
                        BARRIER();
                        ++head->level;

                        #ifdef BG_STATS
                        ++(bg_stats.raises);
                        #endif
                }

                // raise the index level nodes
                for (i = 0; (i+1) < set->head->level; i++) {
                        assert(i < MAX_LEVELS);
                        raised = bg_raise_ilevel(i + 1, ptst);

                        if ((((i+1) == (head->level-1)) && raised)
                                        && head->level < MAX_LEVELS) {
                                // add a new index level

                                // nullify BEFORE we increase the level
                                head->succs[IDX(head->level,zero)] = NULL;
                                BARRIER();
                                ++head->level;

                                #ifdef BG_STATS
                                ++(bg_stats.raises);
                                #endif
                        }
                }

                // if needed, remove the lowest index level
                threshold = bg_non_deleted * 10;
                if (bg_tall_deleted > threshold) {
                        if (head->level > 1) {
                                bg_lower_ilevel(ptst);

                                #ifdef BG_STATS
                                ++(bg_stats.lowers);
                                #endif
                        }
                }

                if (bg_deleted > bg_non_deleted * 3) {
                        bg_should_delete = 1;
                        bg_stats.should_delete += 1;
                }
                else {
                        bg_should_delete = 0;
                }
                BARRIER();
        }

        return NULL;
}

/**
 * bg_trav_nodes - traverse node level of skip list
 * @ptst: per-thread state
 *
 * Returns 1 if a raise was done and 0 otherwise.
 *
 * Note: this tries to raise non-deleted nodes, and finished deletions that
 * have been started but not completed.
 */
static int bg_trav_nodes(ptst_t *ptst)
{
        node_t *prev, *node, *next;
        node_t *above_head = set->head, *above_prev, *above_next;
        unsigned long zero = sl_zero;
        int raised = 0;

        assert(NULL != set && NULL != set->head);

        ptst = ptst_critical_enter();

        above_prev = above_next = above_head;
        prev = set->head;
        node = prev->next;
        if (NULL == node)
                return 0;
        next = node->next;

        while (NULL != next) {

                if (NULL == node->val) {
                        bg_remove(prev, node, ptst);
                        if (node->level >= 1)
                                ++bg_tall_deleted;
                        ++bg_deleted;
                }
                else if (node->val != node) {
                        if ((((0 == prev->level
                                && 0 == node->level)
                                && 0 == next->level))
                                && CAS(&node->raise_or_remove, 0, 1)) {

                                node->level = 1;

                                raised = 1;

                                get_index_above(above_head, &above_prev,
                                                &above_next, 0, node->key,
                                                zero);

                                // swap the pointers
                                node->succs[IDX(0,zero)] = above_next;

                                BARRIER(); // make sure above happens first

                                above_prev->succs[IDX(0,zero)] = node;
                                above_next = above_prev = above_head = node;
                        }
                }

                if (NULL != node->val && node != node->val) {
                        ++bg_non_deleted;
                }
                prev = node;
                node = next;
                next = next->next;
        }

        ptst_critical_exit(ptst);

        return raised;
}

/**
 * get_index_above - get the index node above for a raise
 */
static void get_index_above(node_t *above_head,
                            node_t **above_prev,
                            node_t **above_next,
                            unsigned long i,
                            unsigned long key,
                            unsigned long zero)
{
        /* get the correct index node above */
        while (*above_next && (*above_next)->key < key) {
                *above_next = (*above_next)->succs[IDX(i,zero)];
                if (*above_next != above_head->succs[IDX(i,zero)])
                        *above_prev = (*above_prev)->succs[IDX(i,zero)];
        }
}



/**
 * bg_raise_ilevel - raise the index levels
 * @h: the height of the level we are raising
 * @ptst: per-thread state
 *
 * Returns 1 if a node was raised and 0 otherwise.
 */
static int bg_raise_ilevel(int h, ptst_t *ptst)
{
        int raised = 0;
        unsigned long zero = sl_zero;
        node_t *index, *inext, *iprev = set->head;
        node_t *above_next, *above_prev, *above_head;

        ptst = ptst_critical_enter();

        above_next = above_prev = above_head = set->head;

        index = iprev->succs[IDX(h-1,zero)];
        if (NULL == index)
                return raised;

        while (NULL != (inext = index->succs[IDX(h-1,zero)])) {
                while (index->val == index) {

                        // skip deleted nodes
                        iprev->succs[IDX(h-1,zero)] = inext;
                        BARRIER(); // do removal before level decrementing
                        --index->level;

                        if (NULL == inext)
                                break;
                        index = inext;
                        inext = inext->succs[IDX(h-1,zero)];
                }
                if (NULL == inext)
                        break;
                if ( (((iprev->level <= h) && (index->level == h)) &&
                    (inext->level <= h)) && (index->val != index && NULL != index->val) ) {
                        raised = 1;

                        /* find the correct index node above */
                        get_index_above(above_head, &above_prev, &above_next,
                                        h, index->key, zero);

                        /* fix the pointers and levels */
                        index->succs[IDX(h,zero)] = above_next;
                        BARRIER(); /* link index to above_next first */
                        above_prev->succs[IDX(h,zero)] = index;
                        ++index->level;

                        assert(index->level == h+1);

                        above_next = above_prev = above_head = index;
                }
                iprev = index;
                index = index->succs[IDX(h-1,zero)];
        }

        ptst_critical_exit(ptst);

        return raised;
}

/**
 * bg_lower_ilevel - lower the index level
 */
void bg_lower_ilevel(ptst_t *ptst)
{
        unsigned long zero = sl_zero;
        node_t  *node = set->head;
        node_t  *node_next = node;

        ptst = ptst_critical_enter();

        if (node->level-2 <= sl_zero)
                return; /* no more room to lower */

        /* decrement the level of all nodes */

        while (node) {
                node_next = node->succs[IDX(0,zero)];
                if (!node->marker) {
                        if (node->level > 0) {
                                if (1 == node->level && node->raise_or_remove)
                                        node->raise_or_remove = 0;
                                //BARRIER();
                                /* null out the ptr for level being removed */
                                node->succs[IDX(0,zero)] = NULL;
                                --node->level;
                        }
                }
                node = node_next;
        }

        /* remove the lowest index level */
        BARRIER(); /* do all of the above first */
        ++sl_zero;

        ptst_critical_exit(ptst);
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
        bg_stats.delete_attempts = 0;
        bg_stats.delete_succeeds = 0;
}

/**
 * bg_start - start the background thread
 *
 * Note: Only start the background thread if it is not currently
 * running.
 */
void bg_start(int sleep_time)
{
        /* XXX not thread safe  XXX */
        if (!bg_running) {
                bg_sleep_time = sleep_time;
                bg_running = 1;
                bg_finished = 0;
                pthread_create(&bg_thread, NULL, bg_loop, NULL);
        }
}

/**
 * bg_stop - stop the background thread
 */
void bg_stop(void)
{
        /* XXX not thread safe XXX */
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
        printf("Loops = %lu\n", bg_stats.loops);
        printf("Raises = %lu\n", bg_stats.raises);
        printf("Levels = %lu\n", set->head->level);
        printf("Lowers = %lu\n", bg_stats.lowers);
        printf("Delete Attempts = %lu\n", bg_stats.delete_attempts);
        printf("Delete Succeeds = %lu\n", bg_stats.delete_succeeds);
        printf("Should delete = %lu\n", bg_stats.should_delete);
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
 * by pointing @prev past these nodes.
 */
void bg_help_remove(node_t *prev, node_t *node, ptst_t *ptst)
{
        node_t *n, *new, *prev_next;
        int retval;

        assert(NULL != prev);
        assert(NULL != node);

        if (node->val != node || node->marker)
                return;

        n = node->next;
        while (NULL == n || !n->marker) {
                        new = marker_new(node, n, ptst);
                        CAS(&node->next, n, new);

                        assert (node->next != node);

                        n = node->next;
        }

        #ifdef BG_STATS
        ADD_TO(bg_stats.delete_attempts, 1);
        #endif

        if (prev->next != node || prev->marker)
                return;

        /* remove the nodes */
        retval = CAS(&prev->next, node, n->next);
        assert (prev->next != prev);

        if (retval) {

                node_delete(node, ptst);
                marker_delete(n, ptst);

                #ifdef BG_STATS
                ADD_TO(bg_stats.delete_succeeds, 1);
                #endif
        }

        /*
         * update the prev pointer - we don't need synchronisation here
         * since the prev pointer does not need to be exact
         */
        prev_next = prev->next;
        if (NULL != prev_next)
                prev_next->prev = prev;
}

/**
 * bg_remove - start the physical removal of @node
 * @prev: the node before the one to remove
 * @node: the node to remove
 *
 * Note: we only remove nodes that are of height 1 (i.e. they
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
