/* 
 * $Id$ 
 */

#include "c_slot.h"
#include "c_elem.h"
#include "utils.h"
#include "../../dprint.h"


/*
 * Initialize cache slot structure
 */
int init_slot(cache_t* _c, c_slot_t* _slot)
{
#ifdef PARANOID
	if (!_slot) {
		LOG(L_ERR, "init_slot(): Invalid _slot parameter value\n");
		return FALSE;
	}
	if (!_c) {
		LOG(L_ERR, "init_slot(): Invalid _c parameter value\n");
		return FALSE;
	}
#endif
	SLOT_ELEM_COUNT(_slot) = 0;
	SLOT_FIRST_ELEM(_slot) = NULL;
	SLOT_LAST_ELEM(_slot) = NULL;
	SLOT_CACHE(_slot) = _c;

	init_lock(SLOT_LOCK(_slot));
	return TRUE;
}


/*
 * Deinitialize given slot structure
 */
void deinit_slot(c_slot_t* _slot)
{
	c_elem_t* ptr;
#ifdef PARANOID
	if (!_slot) {
		LOG(L_ERR, "deinit_slot(): Invalid _slot parameter value\n");
		return;
	}
#endif
	while(SLOT_FIRST_ELEM(_slot)) {
		ptr = SLOT_FIRST_ELEM(_slot);
		SLOT_FIRST_ELEM(_slot) = ELEM_SLOT_NEXT(ptr);

		free_element(ptr);
	}
	SLOT_ELEM_COUNT(_slot) = 0;
	SLOT_LAST_ELEM(_slot) = NULL;
	SLOT_CACHE(_slot) = NULL;
}


/*
 * Add an element to an slot's linked list
 */
void slot_add_elem(c_slot_t* _slot, c_elem_t* _el)
{
	if (!SLOT_ELEM_COUNT(_slot)++) {
		SLOT_FIRST_ELEM(_slot) = SLOT_LAST_ELEM(_slot) = _el;
	} else {
		ELEM_SLOT_PREV(_el) = SLOT_LAST_ELEM(_slot);
		ELEM_SLOT_NEXT(SLOT_LAST_ELEM(_slot)) = _el;
		SLOT_LAST_ELEM(_slot) = _el;
	}
	
	ELEM_SLOT(_el) = _slot;
}


/*
 * Remove an element from slot linked list
 */
c_elem_t* slot_rem_elem(c_elem_t* _el)
{
	c_slot_t* slot = ELEM_SLOT(_el);

	if (ELEM_SLOT_PREV(_el)) {
		ELEM_SLOT_NEXT(ELEM_SLOT_PREV(_el)) = ELEM_SLOT_NEXT(_el);
	} else {
		SLOT_FIRST_ELEM(slot) = ELEM_SLOT_NEXT(_el);
	}
	if (ELEM_SLOT_NEXT(_el)) {
		ELEM_SLOT_PREV(ELEM_SLOT_NEXT(_el)) = ELEM_SLOT_PREV(_el);
	} else {
		SLOT_LAST_ELEM(slot) = ELEM_SLOT_PREV(_el);
	}
	ELEM_SLOT_PREV(_el) = ELEM_SLOT_NEXT(_el) = NULL;
	ELEM_SLOT(_el) = NULL;
	SLOT_ELEM_COUNT(slot)--;
	
	return _el;
}


