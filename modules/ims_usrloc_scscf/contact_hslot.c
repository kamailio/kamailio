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

#include "contact_hslot.h"

/*! number of locks */
int contacts_locks_no=4;
/*! global list of locks */
gen_lock_set_t* contacts_locks=0;


/*!
 * \brief Initialize locks for the hash table
 * \return 0 on success, -1 on failure
 */
int init_contacts_locks(void)
{
	int i;
	i = contacts_locks_no;
	do {
		if ((( contacts_locks=lock_set_alloc(i))!=0)&&
				(lock_set_init(contacts_locks)!=0))
		{
			contacts_locks_no = i;
			LM_INFO("locks array size %d\n", contacts_locks_no);
			return 0;

		}
		if (contacts_locks){
			lock_set_dealloc(contacts_locks);
			contacts_locks=0;
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
void unlock_contacts_locks(void)
{
	unsigned int i;

	if (contacts_locks==0)
		return;

	for (i=0;i<contacts_locks_no;i++) {
#ifdef GEN_LOCK_T_PREFERED
		lock_release(&contacts_locks->locks[i]);
#else
		ul_release_idx(i);
#endif
	};
}


/*!
 * \brief Destroy all locks on the list
 */
void destroy_contacts_locks(void)
{
	if (contacts_locks !=0){
		lock_set_destroy(contacts_locks);
		lock_set_dealloc(contacts_locks);
	};
}

#ifndef GEN_LOCK_T_PREFERED
/*!
 * \brief Lock a lock with a certain index
 * \param idx lock index
 */
void lock_contacts_idx(int idx)
{
	lock_set_get(contacts_locks, idx);
}


/*!
 * \brief Release a lock with a certain index
 * \param idx lock index
 */
void release_contacts_idx(int idx)
{
	lock_set_release(contacts_locks, idx);
}
#endif

/*!
 * \brief Initialize cache slot structure
 * \param _d domain for the hash slot
 * \param _s hash slot
 * \param n used to get the slot number (modulo number or locks)
 */
void init_contact_slot(contact_hslot_t* _s, int n)
{
	_s->n = 0;
	_s->first = 0;
	_s->last = 0;
//	_s->d = _d;

#ifdef GEN_LOCK_T_PREFERED
	_s->lock = &contacts_locks->locks[n%contacts_locks_no];
#else
	_s->lockidx = n%contacts_locks_no;
#endif
}


/*!
 * \brief Deinitialize given slot structure
 * \param _s hash slot
 */
void deinit_contact_slot(contact_hslot_t* _s)
{
	struct ucontact* ptr;
	
	     /* Remove all elements */
	while(_s->first) {
		ptr = _s->first;
		_s->first = _s->first->next;
		free_ucontact(ptr);
	}
	
	_s->n = 0;
	_s->last = 0;
//    _s->d = 0;
}


/*!
 * \brief Add an element to an slot's linked list
 * \param _s hash slot
 * \param _r added record
 */
void contact_slot_add(contact_hslot_t* _s, struct ucontact* _c)
{
	if (_s->n == 0) {
		_s->first = _s->last = _c;
	} else {
		_c->prev = _s->last;
		_s->last->next = _c;
		_s->last = _c;
	}
	_s->n++;
	_c->slot = _s;
}


/*!
 * \brief Remove an element from slot linked list
 * \param _s hash slot
 * \param _r removed record
 */
void contact_slot_rem(contact_hslot_t* _s, struct ucontact* _c)
{
	if (_c->prev) {
		_c->prev->next = _c->next;
	} else {
		_s->first = _c->next;
	}

	if (_c->next) {
		_c->next->prev = _c->prev;
	} else {
		_s->last = _c->prev;
	}

	_c->prev = _c->next = 0;
	_c->slot = 0;
	_s->n--;
}

