/* 
 * $Id$ 
 */

#ifndef __C_SLOT_H__
#define __C_SLOT_H__

#include "c_elem.h"
#include "cache.h"
#include "../../fastlock.h"

struct cache;
struct c_elem;

typedef struct c_slot {
	struct {                  /* Linked list of elements in this collision slot */
		int count;        /* Number of elements in the collision slot */
		struct c_elem* first;  /* First element in the list */
		struct c_elem* last;   /* Last element in the list */
	} ll;

	struct cache* cache;           /* Cache we belong to */
	fl_lock_t lock;                /* Cache slot lock */
} c_slot_t;


/* First element the collision slot */
#define SLOT_FIRST_ELEM(slot) ((slot)->ll.first)

/* Number of elements in the collision slot */
#define SLOT_ELEM_COUNT(slot) ((slot)->ll.count)

/* Last element in the collision slot */
#define SLOT_LAST_ELEM(slot) ((slot)->ll.last)

/* Cache we belong to */
#define SLOT_CACHE(slot) ((slot)->cache)

/* Collision slot lock */
#define SLOT_LOCK(slot) ((slot)->lock)


/*
 * Initialize cache slot structure
 */
int init_slot(struct cache* _c, c_slot_t* _ent);


/*
 * Deinitialize given slot structure
 */
void deinit_slot(c_slot_t* _ent);


/*
 * Add an element to slot linked list
 */
void slot_add_elem(c_slot_t* _slot, struct c_elem* _el);


/*
 * Remove an element from slot linked list
 */
struct c_elem* slot_rem_elem(struct c_elem* _el);


#endif
