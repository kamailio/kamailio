/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 *
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fraunhofer FOKUS Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 *
 * NB: A lot of this code was originally part of OpenIMSCore,
 * FhG Fokus.
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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
 *
 */

#include "hslot.h"

/*! number of locks */
int ul_locks_no = 4;
/*! global list of locks */
gen_lock_set_t *ul_locks = 0;

int ul_init_locks(void)
{
	int i;
	i = ul_locks_no;
	do {
		if(((ul_locks = lock_set_alloc(i)) != 0)
				&& (lock_set_init(ul_locks) != 0)) {
			ul_locks_no = i;
			LM_INFO("locks array size %d\n", ul_locks_no);
			return 0;
		}
		if(ul_locks) {
			lock_set_dealloc(ul_locks);
			ul_locks = 0;
		}
		i--;
		if(i == 0) {
			LM_ERR("failed to allocate locks\n");
			return -1;
		}
	} while(1);
}


void ul_unlock_locks(void)
{
	unsigned int i;

	if(ul_locks == 0)
		return;

	for(i = 0; i < ul_locks_no; i++) {
#ifdef GEN_LOCK_T_PREFERED
		lock_release(&ul_locks->locks[i]);
#else
		ul_release_idx(i);
#endif
	};
}

void ul_destroy_locks(void)
{
	if(ul_locks != 0) {
		lock_set_destroy(ul_locks);
		lock_set_dealloc(ul_locks);
	};
}

#ifndef GEN_LOCK_T_PREFERED

void ul_lock_idx(int idx)
{
	lock_set_get(ul_locks, idx);
}

void ul_release_idx(int idx)
{
	lock_set_release(ul_locks, idx);
}
#endif

void init_slot(struct udomain *_d, hslot_t *_s, int n)
{
	_s->n = 0;
	_s->first = 0;
	_s->last = 0;
	_s->d = _d;

#ifdef GEN_LOCK_T_PREFERED
	_s->lock = &ul_locks->locks[n % ul_locks_no];
#else
	_s->lockidx = n % ul_locks_no;
#endif
}

void deinit_slot(hslot_t *_s)
{
	struct pcontact *ptr;

	/* Remove all elements */
	while(_s->first) {
		ptr = _s->first;
		_s->first = _s->first->next;
		free_pcontact(ptr);
	}

	_s->n = 0;
	_s->last = 0;
	_s->d = 0;
}

void slot_add(hslot_t *_s, struct pcontact *_r)
{
	if(_s->n == 0) {
		_s->first = _s->last = _r;
	} else {
		_r->prev = _s->last;
		_s->last->next = _r;
		_s->last = _r;
	}
	_s->n++;
	_r->slot = _s;
}

void slot_rem(hslot_t *_s, struct pcontact *_r)
{
	if(_r->prev) {
		_r->prev->next = _r->next;
	} else {
		_s->first = _r->next;
	}

	if(_r->next) {
		_r->next->prev = _r->prev;
	} else {
		_s->last = _r->prev;
	}

	_r->prev = _r->next = 0;
	_r->slot = 0;
	_s->n--;
}
