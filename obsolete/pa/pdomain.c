/*
 * Presence Agent, domain support
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
/*
 * History:
 * --------
 *  2003-03-11  converted to the new locking scheme: locking.h (andrei)
 */


#include "pdomain.h"
#include "paerrno.h"
#include "presentity.h"
#include "../../ut.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include <cds/logger.h>
#include "pa_mod.h"
#include <time.h>

/*
 * Hash function
 */
static inline unsigned int hash_func(pdomain_t* _d, unsigned char* _s, int _l)
{
	unsigned int res = 0, i;
	
	for(i = 0; i < _l; i++) {
		res += _s[i];
	}
	
	return res % _d->size;
}


/*
 * Create a new domain structure
 * _n is pointer to str representing
 * name of the domain, the string is
 * not copied, it should point to str
 * structure stored in domain list
 * _s is hash table size
 */
int new_pdomain(str* _n, int _s, pdomain_t** _d, register_watcher_t _r, unregister_watcher_t _u)
{
	int i;
	pdomain_t* ptr;
	
	ptr = (pdomain_t*)mem_alloc(sizeof(pdomain_t));
	if (!ptr) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_pdomain(): No memory left\n");
		return -1;
	}
	memset(ptr, 0, sizeof(pdomain_t));
	
	ptr->table = (hslot_t*)mem_alloc(sizeof(hslot_t) * _s);
	if (!ptr->table) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_pdomain(): No memory left 2\n");
		mem_free(ptr);
		return -2;
	}

	ptr->name = _n;
	
	for(i = 0; i < _s; i++) {
		init_slot(ptr, &ptr->table[i]);
	}

	ptr->size = _s;
	lock_init(&ptr->lock);
	ptr->users = 0;
	ptr->expired = 0;
	
	ptr->reg = _r;
	ptr->unreg = _u;
	*_d = ptr;
	return 0;
}


/*
 * Free all memory allocated for
 * the domain
 */
void free_pdomain(pdomain_t* _d)
{
	int i;
	
	lock_pdomain(_d);
	if (_d->table) {
		for(i = 0; i < _d->size; i++) {
			deinit_slot(_d->table + i);
		}
		mem_free(_d->table);
	}
	unlock_pdomain(_d);

        mem_free(_d);
}

int timer_pdomain(pdomain_t* _d)
{
	struct presentity* presentity, *t;
	time_t start, stop;

	PROF_START(pa_timer_pdomain)
	lock_pdomain(_d);
	start = time(NULL);

	presentity = _d->first;

	while(presentity) {
		if (timer_presentity(presentity) < 0) {
			LOG(L_ERR, "timer_pdomain(): Error in timer_pdomain\n");
			unlock_pdomain(_d);
			PROF_STOP(pa_timer_pdomain)
			return -1;
		}
		
		/* Remove the entire record
		 * if it is empty
		 */
		if ( (!presentity->first_watcher) && 
				(!presentity->first_winfo_watcher) && 
				(!presentity->data.first_tuple) &&
				(!presentity->data.first_note) &&
				(!presentity->data.first_unknown_element) &&
				(!presentity->first_qsa_subscription) &&
				(presentity->ref_cnt == 0)) {
			LOG(L_DBG, "timer_pdomain(): removing empty presentity\n");
			t = presentity;
			presentity = presentity->next;
			release_presentity(t);
		} else {
			presentity = presentity->next;
		}
	}
	
	stop = time(NULL);
	if (stop - start > 1) WARN("timer_pdomain took %d seconds\n", (int) (stop - start));
			
	unlock_pdomain(_d);
	PROF_STOP(pa_timer_pdomain)
	return 0;
}


static int in_pdomain = 0; /* this only works with single or multiprocess execution model, but not multi-threaded */

/*
 * Get lock if this process does not already have it
 */
void lock_pdomain(pdomain_t* _d)
{
	DBG("lock_pdomain\n");
	if (!in_pdomain++)
	     lock_get(&_d->lock);
}


/*
 * Release lock
 */
void unlock_pdomain(pdomain_t* _d)
{
	DBG("unlock_pdomain\n");
	in_pdomain--;
	if (!in_pdomain)
	     lock_release(&_d->lock);
}

/*
 * Find a presentity in domain according to uid
 */
int find_presentity_uid(pdomain_t* _d, str* uid, struct presentity** _p)
{
	unsigned int sl, i;
	struct presentity* p;
	int res = 1;

	if ((!uid) || (!_p)) return -1;

	sl = hash_func(_d, (unsigned char *)uid->s, uid->len);
	
	p = _d->table[sl].first;
	
	for(i = 0; i < _d->table[sl].n; i++) {
		if ((p->uuid.len == uid->len) && !memcmp(p->uuid.s, uid->s, uid->len)) {
			*_p = p;
			res = 0;
			break;
		}
		p = p->next;
	}
	
	return res;   /* Nothing found */
}

/*
 * contact will be NULL if user is offline
 */
static void callback(str* _user, str *_contact, int state, void* data)
{
	mq_message_t *msg;
	tuple_change_info_t *info;

	if ((!_user) || (!_contact) || (!data)) {
		ERROR_LOG("callback(): error!\n");
	}
		
	/* asynchronous processing */
	msg = create_message_ex(sizeof(tuple_change_info_t));
	if (!msg) {
		LOG(L_ERR, "can't create message with tuple status change\n");
		return;
	}
	set_data_destroy_function(msg, (destroy_function_f)free_tuple_change_info_content);
	info = get_message_data(msg);
	if (state == 0) info->state = presence_tuple_closed;
	else info->state = presence_tuple_open;
	str_dup(&info->user, _user);
	str_dup(&info->contact, _contact);
	if (data) push_message(&((struct presentity*)data)->mq, msg);
}

void add_presentity(pdomain_t* _d, struct presentity* _p)
{
	unsigned int sl;

	sl = hash_func(_d, (unsigned char *)_p->uuid.s, _p->uuid.len);

	slot_add(&_d->table[sl], _p, &_d->first, &_d->last);

	if (use_callbacks) {
		DBG("! registering callback to %.*s, %p\n", _p->uuid.len, _p->uuid.s,_p);
		_d->reg(&_p->data.uri, &_p->uuid, (void*)callback, _p);
	}
	if (subscribe_to_users) {
		TRACE("! subscribing to %.*s, %p\n", _p->uuid.len, _p->uuid.s,_p);
		subscribe_to_user(_p);
	}
}


void remove_presentity(pdomain_t* _d, struct presentity* _p)
{
	if (use_callbacks) {
		DBG("! unregistering callback to %.*s, %p\n", _p->uuid.len, _p->uuid.s,_p);
		_d->unreg(&_p->data.uri, &_p->uuid, (void*)callback, _p);
		DBG("! unregistered callback to %.*s, %p\n", _p->uuid.len, _p->uuid.s,_p);
	}
	if (subscribe_to_users) {
		DBG("! unsubscribing from %.*s, %p\n", _p->uuid.len, _p->uuid.s,_p);
		unsubscribe_to_user(_p);
	}
	
	LOG(L_DBG, "remove_presentity _p=%p p_uri=%.*s\n", _p, _p->data.uri.len, 
			_p->data.uri.s);
	slot_rem(_p->slot, _p, &_d->first, &_d->last);

	/* remove presentity from database */
}

