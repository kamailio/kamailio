/* 
 * $Id$ 
 */

#include "c_slot.h"
#include "c_elem.h"
#include "utils.h"


int init_slot(cache_t* _c, c_slot_t* _slot)
{
#ifdef PARANOID
	if (!_slot) return FALSE;
	if (!_c) return FALSE;
#endif
	_slot->ll.count = 0;
	_slot->ll.first = NULL;
	_slot->ll.last = NULL;
	_slot->mutex = 0;
	_slot->ref = 0;
	_slot->cache = _c;
	return TRUE;
}



void deinit_slot(c_slot_t* _slot)
{
	c_elem_t* ptr;
#ifdef PARANOID
	if (!_slot) return;
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

