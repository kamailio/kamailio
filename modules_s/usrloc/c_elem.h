/* 
 * $Id$ 
 */

#ifndef __C_ELEM_H__
#define __C_ELEM_H__

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
	}ll;

	location_t* loc;              /* Element payload */

	struct c_slot* ht_slot;       /* Collision slot in the hash table array we belong to */

	struct {                      /* State of this element */
		int ref;              /* Reference count */
		int mutex;            /* Mutex if we have enough mutexes */
		int garbage;          /* Means that the element should be deleted asap */
		int invisible;        /* Means that element is invisible to all ops */
	} state;

	struct {                      /* Linked list off all elements in hash table */
		struct c_elem* prev;  /* Previous item in the list */
		struct c_elem* next;  /* Next item in the list */
	} c_ll;
} c_elem_t;

c_elem_t* create_element (location_t* _loc);
void      free_element   (c_elem_t* _el);

int       add_slot_elem (struct c_slot* _slot, c_elem_t* _el);
c_elem_t* rem_slot_elem (struct c_slot* _slot, c_elem_t* _el);

int       add_cache_elem (struct cache* _c, c_elem_t* _el);
c_elem_t* rem_cache_elem (struct cache* _c, c_elem_t* _el);

void print_element(c_elem_t* _el);


#endif
