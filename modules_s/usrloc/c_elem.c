/* 
 * $Id$ 
 */

#include "c_elem.h"
#include <stdlib.h>
#include <stdio.h>   /* remove */
#include "utils.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "sh_malloc.h"

/*
 * Create new element structure and initialize members
 */
c_elem_t* create_element(location_t* _loc)
{
	c_elem_t* ptr;

	ptr = (c_elem_t*)sh_malloc(sizeof(c_elem_t));
	if (!ptr) {
		LOG(L_ERR, "create_element(): No memory left\n");
		return NULL;
	}

	ELEM_SLOT_NEXT(ptr) = ELEM_SLOT_PREV(ptr) = NULL;
	ELEM_LOC(ptr) = _loc;
	ELEM_SLOT(ptr) = NULL;
	ELEM_CACHE_PREV(ptr) = ELEM_CACHE_NEXT(ptr) = NULL;
	
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

	if (ELEM_LOC(_el)) free_location(ELEM_LOC(_el));
	sh_free(_el);
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

	LOG(L_ERR, "usr_loc: Nothing to print right now\n");
	LOG(L_ERR, "usr_loc: location:\n");
	print_location(ELEM_LOC(_el));
}
