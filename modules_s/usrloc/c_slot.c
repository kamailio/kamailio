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
	_slot->ll.count = 0;
	_slot->ll.first = NULL;
	_slot->ll.last = NULL;
	_slot->cache = _c;

	init_lock(_slot->lock);
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
	while(_slot->ll.first) {
		ptr = _slot->ll.first;
		_slot->ll.first = ptr->ll.next;

		free_element(ptr);
	}
	_slot->ll.count = 0;
	_slot->ll.last = NULL;
	_slot->cache = NULL;
}

