/* 
 * $Id$ 
 */

#include "c_elem.h"
#include <stdlib.h>
#include <stdio.h>   /* remove */
#include "utils.h"
#include "../../dprint.h"
#include "../../mem/mem.h"

/*
 * Create new element structure and initialize members
 */
c_elem_t* create_element(location_t* _loc)
{
	c_elem_t* ptr;

	ptr = (c_elem_t*)pkg_malloc(sizeof(c_elem_t));
	if (!ptr) {
		LOG(L_ERR, "create_element(): No memory left\n");
		return NULL;
	}

	ptr->ll.next = ptr->ll.prev = NULL;
	ptr->loc = _loc;
	ptr->ht_slot = NULL;
	ptr->c_ll.prev = ptr->c_ll.next = NULL;
	
	return ptr;
}


/*
 * Dispose an element
 * Must be removed from all linked lists !
 */
void free_element(c_elem_t* _el)
{
#ifdef PARANOID
	if (!_el) return;
#endif

	if (_el->loc) free_location(_el->loc);
	pkg_free(_el);
}


/*
 * Add an element to an slot's linked list
 */
int add_slot_elem(c_slot_t* _slot, c_elem_t* _el)
{
#ifdef PARANOID
	if (!_slot) {
		LOG(L_ERR, "add_slot_elem(): Invalid _slot parameter value\n");
		return FALSE;
	}
	if (!_el) { 
		LOG(L_ERR, "add_slot_elem(): Invalid _el parameter value\n");
		return FALSE;
	}
#endif
	if (!_slot->ll.count++) {
		_slot->ll.first = _slot->ll.last = _el;
	} else {
		_el->ll.prev = _slot->ll.last;
		_slot->ll.last->ll.next = _el;
		_slot->ll.last = _el;
	}

	_el->ht_slot = _slot;
	return TRUE;
}


c_elem_t* rem_slot_elem(c_slot_t* _slot, c_elem_t* _el)
{
	c_elem_t* ptr;
#ifdef PARANOID
	if (!_slot) {
		LOG(L_ERR, "rem_slot_elem(): Invalid _slot parameter value\n");
		return NULL;
	}
	if (!_el) {
		LOG(L_ERR, "rem_slot_elem(): Invalid _el parameter value\n");
		return NULL;
	}
	if (!_slot->ll.count) return NULL;
#endif

	ptr = _slot->ll.first;

	while(ptr) {
		if (ptr == _el) {
			if (ptr->ll.prev) {
				ptr->ll.prev->ll.next = ptr->ll.next;
			} else {
				_slot->ll.first = ptr->ll.next;
			}
			if (ptr->ll.next) {
				ptr->ll.next->ll.prev = ptr->ll.prev;
			} else {
				_slot->ll.last = ptr->ll.prev;
			}
			ptr->ll.prev = ptr->ll.next = NULL;
			ptr->ht_slot = NULL;
			_slot->ll.count--;
			break;
		}
		ptr = ptr->ll.next;
	}
	return ptr;
}



/*
 * Add an element to linked list of all elements in hash table
 */
int add_cache_elem(cache_t* _c, c_elem_t* _el)
{
#ifdef PARANOID
	if (!_c) {
		LOG(L_ERR, "add_cache_elem(): Invalid _c parameter value\n");
		return FALSE;
	}
	if (!_el) {
		LOG(L_ERR, "add_cache_elem(): Invalid _el parameter value\n");
		return FALSE;
	}
#endif

	if (!_c->c_ll.count++) {
		_c->c_ll.first = _c->c_ll.last = _el;
	} else {
		_el->c_ll.prev = _c->c_ll.last;
		_c->c_ll.last->c_ll.next = _el;
		_c->c_ll.last = _el;
	}

	return TRUE;
}


c_elem_t* rem_cache_elem(cache_t* _c, c_elem_t* _el)
{
	c_elem_t* ptr;

#ifdef PARANOID
	if (!_c) {
		LOG(L_ERR, "rem_cache_elem(): Invalid _c parameter value\n");
		return NULL;
	}
	if (!_el) {
		LOG(L_ERR, "rem_cache_elem(): Invalid _el parameter value\n");
		return NULL;
	}
	if (!_c->c_ll.count) return NULL;
#endif
	ptr = _c->c_ll.first;

	while(ptr) {
		if (ptr == _el) {
			if (ptr->c_ll.prev) {
				ptr->c_ll.prev->c_ll.next = ptr->c_ll.next;
			} else {
				_c->c_ll.first = ptr->c_ll.next;
			}
			if (ptr->c_ll.next) {
				ptr->c_ll.next->c_ll.prev = ptr->c_ll.prev;
			} else {
				_c->c_ll.last = ptr->c_ll.prev;
			}
			ptr->c_ll.prev = ptr->c_ll.next = NULL;
			_c->c_ll.count--;
			break;
		}
		ptr = ptr->c_ll.next;
	}
	return ptr;
}
		


/*
 * Print an element, just for debugging purposes
 */
void print_element(c_elem_t* _el)
{
#ifdef PARANOID
	if (!_el) {
		LOG(L_ERR, "print_element(): Invalid _el parameter value\n");
		return;
	}
#endif

	printf("Nothing to print right now\n");
	printf("location:\n");
	print_location(_el->loc);
}
