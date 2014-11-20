#ifndef __SET_H__
#define __SET_H__


typedef unsigned long setkey_t;
typedef void         *setval_t;


#ifdef __SET_IMPLEMENTATION__

/*************************************
 * INTERNAL DEFINITIONS
 */

/* Fine for 2^NUM_LEVELS nodes. */
#define NUM_LEVELS 20


/* Internal key values with special meanings. */
#define INVALID_FIELD   (0)    /* Uninitialised field value.     */
#define SENTINEL_KEYMIN ( 1UL) /* Key value of first dummy node. */
#define SENTINEL_KEYMAX (~0UL) /* Key value of last dummy node.  */


/*
 * Used internally be set access functions, so that callers can use
 * key values 0 and 1, without knowing these have special meanings.
 */
#define CALLER_TO_INTERNAL_KEY(_k) ((_k) + 2)


/*
 * SUPPORT FOR WEAK ORDERING OF MEMORY ACCESSES
 */

#ifdef WEAK_MEM_ORDER

/* Read field @_f into variable @_x. */
#define READ_FIELD(_x,_f)                                       \
do {                                                            \
    (_x) = (_f);                                                \
    if ( (_x) == INVALID_FIELD ) { RMB(); (_x) = (_f); }        \
    assert((_x) != INVALID_FIELD);                              \
} while ( 0 )

#else

/* Read field @_f into variable @_x. */
#define READ_FIELD(_x,_f) ((_x) = (_f))

#endif


#else

/*************************************
 * PUBLIC DEFINITIONS
 */

/*
 * Key range accepted by set functions.
 * We lose three values (conveniently at top end of key space).
 *  - Known invalid value to which all fields are initialised.
 *  - Sentinel key values for up to two dummy nodes.
 */
#define KEY_MIN  ( 0U)
#define KEY_MAX  ((~0U) - 3)

typedef void set_t; /* opaque */

void _init_set_subsystem(void);

/*
 * Allocate an empty set.
 */
set_t *set_alloc(void);

/*
 * Add mapping (@k -> @v) into set @s. Return previous mapped value if
 * one existed, or NULL if no previous mapping for @k existed.
 * 
 * If @overwrite is FALSE, then if a mapping already exists it is not
 * modified, and the existing value is returned unchanged. It is possible
 * to see if the value was changed by observing if the return value is NULL.
 */
/*setval_t*/ int set_update(set_t *s, setkey_t k, setval_t v, int overwrite);

/*
 * Remove mapping for key @k from set @s. Return value associated with
 * removed mapping, or NULL is there was no mapping to delete.
 */
/*setval_t*/ int set_remove(set_t *s, setkey_t k);

/*
 * Look up mapping for key @k in set @s. Return value if found, else NULL.
 */
/*setval_t*/ int set_lookup(set_t *s, setkey_t k);

void set_print(set_t *set);
unsigned long set_count(set_t *set);
void set_print_nodenums(set_t *set);

#endif /* __SET_IMPLEMENTATION__ */


#endif /* __SET_H__ */
