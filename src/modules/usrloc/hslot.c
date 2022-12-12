/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*! \file
 *  \brief USRLOC - Hash table collision slot related functions
 *  \ingroup usrloc
 *
 * - Module: \ref usrloc
 */



#include "hslot.h"

/*!
 * \brief Initialize cache slot structure
 * \param _d domain for the hash slot
 * \param _s hash slot
 * \param n used to get the slot number (modulo number or locks)
 */
int init_slot(struct udomain* _d, hslot_t* _s, int n)
{
	_s->n = 0;
	_s->first = 0;
	_s->last = 0;
	_s->d = _d;
	if(rec_lock_init(&_s->rlock)==NULL) {
		LM_ERR("failed to initialize the slock (%d)\n", n);
		return -1;
	}
	return 0;
}


/*!
 * \brief Deinitialize given slot structure
 * \param _s hash slot
 */
void deinit_slot(hslot_t* _s)
{
	struct urecord* ptr;

	/* Remove all elements */
	while(_s->first) {
		ptr = _s->first;
		_s->first = _s->first->next;
		free_urecord(ptr);
	}
	rec_lock_destroy(&_s->rlock);

	_s->n = 0;
	_s->last = 0;
    _s->d = 0;
}


/*!
 * \brief Add an element to a slot's linked list
 * \param _s hash slot
 * \param _r added record
 */
void slot_add(hslot_t* _s, struct urecord* _r)
{
	if (_s->n == 0) {
		_s->first = _s->last = _r;
	} else {
		_r->prev = _s->last;
		_s->last->next = _r;
		_s->last = _r;
	}
	_s->n++;
	_r->slot = _s;
}


/*!
 * \brief Remove an element from slot linked list
 * \param _s hash slot
 * \param _r removed record
 */
void slot_rem(hslot_t* _s, struct urecord* _r)
{
	if (_r->prev) {
		_r->prev->next = _r->next;
	} else {
		_s->first = _r->next;
	}

	if (_r->next) {
		_r->next->prev = _r->prev;
	} else {
		_s->last = _r->prev;
	}

	_r->prev = _r->next = 0;
	_r->slot = 0;
	_s->n--;
}
