/*
 * Presence Agent, hash table
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "hslot.h"


/*
 * Initialize cache slot structure
 */
void init_slot(struct pdomain* _d, hslot_t* _s)
{
	_s->n = 0;
	_s->first = 0;
	_s->d = _d;
}


/*
 * Deinitialize given slot structure
 */
void deinit_slot(hslot_t* _s)
{
	presentity_t* ptr;
	
	/* Remove all elements */
	while ((_s->first) && (_s->n > 0)) {
		ptr = _s->first;
		_s->first = _s->first->next;
		_s->n--;
		free_presentity(ptr);
	}

	_s->n = 0;
	_s->d = 0;
}


/*
 * Add an element to an slot's linked list
 */
void slot_add(hslot_t* _s, struct presentity* _p, struct presentity** _f, struct presentity** _l)
{
	if (_s->first) {
		_p->next = _s->first;
		_p->prev = _s->first->prev;
		_s->first->prev = _p;
		
		if (_p->prev) _p->prev->next = _p;
		else *_f = _p;
	} else {
		if (*_l) {
			(*_l)
->next = _p;
			_p->prev = *_l;
			*_l = _p;
		} else {
			*_l = _p;
			*_f = _p;
		}
	}
	_s->first = _p;
	_p->slot = _s;
	_s->n ++;
}


/*
 * Remove an element from slot linked list
 */
void slot_rem(hslot_t* _s, struct presentity* _p, struct presentity** _f, struct presentity** _l)
{
	if (_s->first == _p) {
		if (_p->next && (_p->next->slot == _s)) _s->first = _p->next;
		else _s->first = 0;
	}

	if (_p->prev) {
		_p->prev->next = _p->next;
	} else {
		*_f = _p->next;
	}

	if (_p->next) {
		_p->next->prev = _p->prev;
	} else {
		*_l = _p->prev;
	}

	_s->n--;

	_p->slot = 0;
}
