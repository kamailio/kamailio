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

	struct {                      /* Linked list of all elements in hash table */
		struct c_elem* prev;  /* Previous item in the list */
		struct c_elem* next;  /* Next item in the list */
	} c_ll;
} c_elem_t;


/*
 * Create a new cache element
 */
c_elem_t* create_element(location_t* _loc);


/*
 * Free all memory associated with the element
 */
void free_element(c_elem_t* _el);


/*
 * Add element to cache collision slot
 */
int add_slot_elem(struct c_slot* _slot, c_elem_t* _el);


/*
 * Remove element from cache collision slot
 */
c_elem_t* rem_slot_elem(struct c_slot* _slot, c_elem_t* _el);


/*
 * Add element to linked list of all elements in a cache
 */
int add_cache_elem (struct cache* _c, c_elem_t* _el);


/*
 * Remove element from cache linked list
 */
c_elem_t* rem_cache_elem (struct cache* _c, c_elem_t* _el);


/*
 * Print an element, for debugging purposes only
 */
void print_element(c_elem_t* _el);


#endif
