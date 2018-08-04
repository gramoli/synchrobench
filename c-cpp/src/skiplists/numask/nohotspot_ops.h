/*
 * Interface for the No Hot Spot Non-Blocking Skip List
 * operations.
 *
 * Author: Ian Dick, 2013.
 *
 *
 * NUMASK changes: use search_layer object instead of set struct
 */
#ifndef NOHOTSPOT_OPS_H_
#define NOHOTSPOT_OPS_H_

#include "skiplist.h"
#include "queue.h"
#include "search.h"

enum sl_optype { CONTAINS, DELETE, INSERT };
typedef enum sl_optype sl_optype_t;
int sl_do_operation(search_layer *sl, sl_optype_t optype, sl_key_t key, val_t val);

/* these are macros instead of functions to improve performance */
#define sl_contains(a, b) sl_do_operation((a), CONTAINS, (b), NULL);
#define sl_delete(a, b) sl_do_operation((a), DELETE, (b), NULL);
#define sl_insert(a, b, c) sl_do_operation((a), INSERT, (b), (c));
#define sl_finish_old(a,b,c,d) sl_finish_operation((a), (b), (c), (d));

#endif /* NOHOTSPOT_OPS_H_ */

