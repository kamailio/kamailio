/* 
 * $Id$ 
 *
 * Hash table collision slot related functions
 */

#include "hslot.h"


/*
 * Initialize cache slot structure
 */
int init_slot(struct udomain* _d, hslot_t* _s)
{
	_s->n = 0;
	_s->first = 0;
	_s->last = 0;
	_s->d = _d;
	return 0;
}


/*
 * Deinitialize given slot structure
 */
void deinit_slot(hslot_t* _s)
{
	struct urecord* ptr;
	
	     /* Remove all elements */
	while(_s->first) {
		ptr = _s->first;
		_s->first = _s->first->s_ll.next;
		free_urecord(ptr);
	}
	
	_s->n = 0;
	_s->last = 0;
        _s->d = 0;
}


/*
 * Add an element to an slot's linked list
 */
void slot_add(hslot_t* _s, struct urecord* _r)
{
	if (_s->n == 0) {
		_s->first = _s->last = _r;
	} else {
		_r->s_ll.prev = _s->last;
		_s->last->s_ll.next = _r;
		_s->last = _r;
	}
	_s->n++;
	_r->slot = _s;
}


/*
 * Remove an element from slot linked list
 */
void slot_rem(hslot_t* _s, struct urecord* _r)
{
	if (_r->s_ll.prev) {
		_r->s_ll.prev->s_ll.next = _r->s_ll.next;
	} else {
		_s->first = _r->s_ll.next;
	}

	if (_r->s_ll.next) {
		_r->s_ll.next->s_ll.prev = _r->s_ll.prev;
	} else {
		_s->last = _r->s_ll.prev;
	}

	_r->s_ll.prev = _r->s_ll.next = 0;
	_r->slot = 0;
	_s->n--;
}
