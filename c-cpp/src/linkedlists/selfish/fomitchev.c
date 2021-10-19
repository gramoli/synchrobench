/* 
 * This algorithm is the linked list as published in
 *    Fomitchev and Ruppert
 *    Lock-free linked lists and skip lists
 *    PODC 2004.
 */

#include "intset.h"
#include "fomitchev.h"

// Import common set operations
#include "mixin.c"

// The node.next field in the algorithm is presented as a tuple
// (right, mark, flag) where right is a reference, and the other
// two are booleans. Following the presentation in the paper,
// arrange the tuple in this way. The following functions work
// on tuples.

// Is the tuple marked?
static inline int is_marked(node_t *n) {
  return (int) ((uintptr_t) n & (uintptr_t) 2);
}

// Is the tuple flagged?
static inline int is_flagged(node_t *n) {
  return (int) ((uintptr_t) n & (uintptr_t) 1);
}

// Return the right field of the tuple.
static inline node_t* get_right(node_t *n) {
  return (node_t *) ((uintptr_t) n & ~(uintptr_t) 3);
}

// Assemble a new (right, mark, flag) tuple.
static inline node_t *pack_tuple(node_t *right, int mark, int flag) {
  uintptr_t tuple = (uintptr_t) get_right(right);
  if (flag) tuple |= (uintptr_t) 1;
  if (mark) tuple |= (uintptr_t) 2;
  return (node_t *) tuple;
}

// Function headers for helper functions. Interface functions
// are presented down the bottom of the file.
static void fomitchev_searchfrom(val_t val, node_t *curr_node, node_t **n1, node_t **n2);
static void fomitchev_searchfrom2(val_t val, node_t *curr_node, node_t **n1, node_t **n2);
static void fomitchev_helpmarked(node_t *prev_node, node_t *del_node);
static void fomitchev_helpflagged(node_t *prev_node, node_t *del_node);
static void fomitchev_trymark(node_t *del_node);
static int fomitchev_tryflag(node_t *prev_node, node_t *target_node, node_t** ret_node);

// Searches forward from curr_node to find two nodes n1 and n2,
// satisfying n1.val <= val < n2.val.
static void fomitchev_searchfrom(val_t val, node_t *curr_node, node_t **n1, node_t **n2) {
  node_t *next_node = get_right(curr_node->next);
  while (next_node->val <= val) {
    // Ensure either next_node is unmarked,
    // or both curr_node and next_node are
    // marked and curr_node was marked earlier.
    while (is_marked(next_node->next) &&
           (!is_marked(curr_node->next) || get_right(curr_node->next) != next_node)) {
      if (get_right(curr_node->next) == next_node) {
        fomitchev_helpmarked(curr_node, next_node);
      }
      next_node = get_right(curr_node->next);
    }
    if (next_node->val <= val) {
      curr_node = next_node;
      next_node = get_right(curr_node->next);
    }
  }

  // Return (curr_node, next_node)
  *n1 = curr_node;
  *n2 = next_node;
}

// Searches forward from curr_node to find two nodes n1 and n2,
// satisfying n1.val < val <= n2.val.
static void fomitchev_searchfrom2(val_t val, node_t *curr_node, node_t **n1, node_t **n2) {
  node_t *next_node = get_right(curr_node->next);
  while (next_node->val < val) {
    // Ensure either next_node is unmarked,
    // or both curr_node and next_node are
    // marked and curr_node was marked earlier.
    while (is_marked(next_node->next) &&
           (!is_marked(curr_node->next) || get_right(curr_node->next) != next_node)) {
      if (get_right(curr_node->next) == next_node) {
        fomitchev_helpmarked(curr_node, next_node);
      }
      next_node = get_right(curr_node->next);
    }
    if (next_node->val < val) {
      curr_node = next_node;
      next_node = get_right(curr_node->next);
    }
  }

  // Return (curr_node, next_node)
  *n1 = curr_node;
  *n2 = next_node;
}

// Assumes that prev_node is flagged, del_node is marked, and
// prev_node.right = del_node. Attempts to swing prev_node.right
// to del_node.right.
static void fomitchev_helpmarked(node_t *prev_node, node_t *del_node) {
  node_t *next_node = get_right(del_node->next);
  node_t *expected = pack_tuple(del_node, 0, 1);
  atomic_compare_exchange_strong(
      &prev_node->next,
      &expected,
      pack_tuple(next_node, 0, 0));
}


// Assumes that prev_node is flagged, and prev_node.right = del_node.
// Attempts to mark and then physically del_node.
static void fomitchev_helpflagged(node_t *prev_node, node_t *del_node) {
  del_node->backlink = prev_node;
  if (!is_marked(del_node->next))
    fomitchev_trymark(del_node);
  fomitchev_helpmarked(prev_node, del_node);
}

// Assumes that del_node is preceded by a flagged node.
// Attempts to mark the node del_node.
static void fomitchev_trymark(node_t *del_node) {
  do {
    node_t *next_node = get_right(del_node->next);
    node_t *expected = pack_tuple(next_node, 0, 0);
    atomic_compare_exchange_strong(
        &del_node->next,
        &expected,
        pack_tuple(next_node, 1, 0));
    if (is_flagged(expected)) {
      // Failure due to del_node becoming flagged.
      fomitchev_helpflagged(del_node, get_right(expected));
    }
  } while (!is_marked(del_node->next));
}

// Attempts to flag the predecessor of target_node.
// prev_node is the last known node to be the predecessor.
//
// Upon returning, if ret_node is not null, then ret_node is flagged and precedes target_node.
// If ret_node is null, then some other operation deleted the target_node.
//
// The return value (boolean) is true only if ret_node is not null, and this function call
// was the one who performed the flagging.
static int fomitchev_tryflag(node_t *prev_node, node_t *target_node, node_t** ret_node) {
  for (;;) {
    if (prev_node->next == pack_tuple(target_node, 0, 1)) {
      // Predecessor already flagged. Report fail and return predecessor.
      *ret_node = prev_node;
      return 0;
    }

    // Attempt to flag
    node_t *expected = pack_tuple(target_node, 0, 0);
    atomic_compare_exchange_strong(
        &prev_node->next,
        &expected,
        pack_tuple(target_node, 0, 1));

    if (expected == pack_tuple(target_node, 0, 0)) {
      // Successful flagging
      *ret_node = prev_node;
      return 1;
    }
    if (expected == pack_tuple(target_node, 0, 1)) {
      // Some concurrent op flagged it first.
      *ret_node = prev_node;
      return 0;
    }
    // Possibly a fail due to marking. Follow the backlinks to
    // something unmarked.
    while (is_marked(prev_node->next)) {
      prev_node = prev_node->backlink;
    }

    node_t *del_node;
    fomitchev_searchfrom2(target_node->val, prev_node, &prev_node, &del_node);
    if (del_node != target_node) {
      // Target got deleted
      *ret_node = NULL;
      return 0;
    }
  }
}


// Interface functions.

// Returns boolean of "is val in set?"
int set_contains(intset_t *set, val_t val) {
  node_t *curr_node, *next_node;
  fomitchev_searchfrom(val, set->head, &curr_node, &next_node);
  if (curr_node->val == val)
    return 1;
  return 0;
}

// Inserts val into set. Returns 1 if val was inserted, 0 if it already existed.
int set_insert(intset_t *set, val_t val) {
  node_t *prev_node, *next_node;
  fomitchev_searchfrom(val, set->head, &prev_node, &next_node);
  if (prev_node->val == val)
    return 0;

  node_t *newnode = new_node(val, NULL);
  for (;;) {
    node_t *prev_next = prev_node->next;
    if (is_flagged(prev_next)) {
      fomitchev_helpflagged(prev_node, get_right(prev_next));
    } else {
      newnode->next = pack_tuple(next_node, 0, 0);
      node_t *expected = pack_tuple(next_node, 0, 0);
      atomic_compare_exchange_strong(
          &prev_node->next,
          &expected,
          pack_tuple(newnode, 0, 0));
      if (expected == pack_tuple(next_node, 0, 0)) {
        // Success
        return 1;
      } else {
        // Failure due to flagging?
        if (is_flagged(expected)) {
          fomitchev_helpflagged(prev_node, get_right(expected));
        }
        // May have to go through backlinks due to marking.
        while (is_marked(prev_node->next)) {
          prev_node = prev_node->backlink;
        }
      }
    }
    fomitchev_searchfrom(val, prev_node, &prev_node, &next_node);
    if (prev_node->val == val) {
      // Free newnode? Nah.
      return 0;
    }
  }
}

// Removes val from set. Returns 1 if val was removed, 0 if val was not
// already in there.
int set_remove(intset_t *set, val_t val) {
  node_t *prev_node, *del_node;
  fomitchev_searchfrom2(val, set->head, &prev_node, &del_node);
  if (del_node->val != val) {
    return 0; // No such key
  }
  int result = fomitchev_tryflag(prev_node, del_node, &prev_node);
  if (prev_node != NULL) {
    fomitchev_helpflagged(prev_node, del_node);
  }
  if (!result) {
    return 0; // No such key
  }
  // Could return the deleted node here.
  return 1;
}
