/* 
 * $Id$ 
 */

#ifndef C_ELEM_H
#define C_ELEM_H

#include "location.h"
#include "c_slot.h"
#include "c_elem.h"
#include "cache.h"

struct cache;
struct c_slot;

/*
 * Basic hash table element
 */
typedef struct c_elem {
	struct {
		struct c_elem* prev;  /* Next item in the linked list */
		struct c_elem* next;  /* Previous item in the linked list */
	} ll;

	location_t* loc;              /* Element payload */

	struct c_slot* ht_slot;       /* Collision slot in the hash table array we belong to */

	struct {                      /* Linked list of all elements in hash table */
		struct c_elem* prev;  /* Previous item in the list */
		struct c_elem* next;  /* Next item in the list */
	} c_ll;
} c_elem_t;


/* Previous element in the slot linked list */
#define ELEM_SLOT_PREV(elem) ((elem)->ll.prev)

/* Next element in the slot linked list */
#define ELEM_SLOT_NEXT(elem) ((elem)->ll.next)

/* Location structure */
#define ELEM_LOC(elem) ((elem)->loc)

/* Slot that this element belongs to */
#define ELEM_SLOT(elem) ((elem)->ht_slot)

/* Previous element in the cache linked list */
#define ELEM_CACHE_PREV(elem) ((elem)->c_ll.prev)

/* Next element in the cache linked list */
#define ELEM_CACHE_NEXT(elem) ((elem)->c_ll.next)


/* Create a new cache element */
c_elem_t* create_element(location_t* _loc);


/* Free all memory associated with the element */
void free_element(c_elem_t* _el);

/*
 * Print an element, for debugging purposes only
 */
void print_element(c_elem_t* _el);


#endif
