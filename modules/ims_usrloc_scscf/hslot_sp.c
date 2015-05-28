/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
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

#include "hslot_sp.h"

/*! number of locks */
int subs_locks_no=4;
/*! global list of locks */
gen_lock_set_t* subs_locks=0;


/*!
 * \brief Initialize locks for the hash table
 * \return 0 on success, -1 on failure
 */
int subs_init_locks(void)
{
	int i;
	i = subs_locks_no;
	do {
		if ((( subs_locks=lock_set_alloc(i))!=0)&&
				(lock_set_init(subs_locks)!=0))
		{
			subs_locks_no = i;
			LM_INFO("locks array size %d\n", subs_locks_no);
			return 0;

		}
		if (subs_locks){
			lock_set_dealloc(subs_locks);
			subs_locks=0;
		}
		i--;
		if(i==0)
		{
			LM_ERR("failed to allocate locks\n");
			return -1;
		}
	} while (1);
}


/*!
 * \brief Unlock all locks on the list
 */
void subs_unlock_locks(void)
{
	unsigned int i;

	if (subs_locks==0)
		return;

	for (i=0;i<subs_locks_no;i++) {
#ifdef GEN_LOCK_T_PREFERED
		lock_release(&subs_locks->locks[i]);
#else
		subs_release_idx(i);
#endif
	};
}


/*!
 * \brief Destroy all locks on the list
 */
void subs_destroy_locks(void)
{
	if (subs_locks !=0){
		lock_set_destroy(subs_locks);
		lock_set_dealloc(subs_locks);
	};
}

#ifndef GEN_LOCK_T_PREFERED
/*!
 * \brief Lock a lock with a certain index
 * \param idx lock index
 */
void subs_lock_idx(int idx)
{
	lock_set_get(subs_locks, idx);
}


/*!
 * \brief Release a lock with a certain index
 * \param idx lock index
 */
void subs_release_idx(int idx)
{
	lock_set_release(subs_locks, idx);
}
#endif

/*!
 * \brief Initialize cache slot structure
 * \param _d domain for the hash slot
 * \param _s hash slot
 * \param n used to get the slot number (modulo number or locks)
 */
void subs_init_slot(hslot_sp_t* _s, int n)
{
	_s->n = 0;
	_s->first = 0;
	_s->last = 0;

#ifdef GEN_LOCK_T_PREFERED
	_s->lock = &subs_locks->locks[n%subs_locks_no];
#else
	_s->lockidx = n%subs_locks_no;
#endif
}


/*!
 * \brief Deinitialize given slot structure
 * \param _s hash slot
 */
void subs_deinit_slot(hslot_sp_t* _s)
{
	//struct ims_subscription_s* ptr;

	     /* Remove all elements */
	while(_s->first) {
		//ptr = _s->first;
		_s->first = _s->first->next;
		//free_impurecord(ptr);
	}

	_s->n = 0;
	_s->last = 0;
}


/*!
 * \brief Add an element to an slot's linked list
 * \param _s hash slot
 * \param _r added record
 */
void subs_slot_add(hslot_sp_t* _s, struct ims_subscription_s* _r)
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
void subs_slot_rem(hslot_sp_t* _s, struct ims_subscription_s* _r)
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
