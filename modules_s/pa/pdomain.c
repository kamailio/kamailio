/*
 * Presence Agent, domain support
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "pdomain.h"
#include "paerrno.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"


/*
 * Hash function
 */
static inline int hash_func(pdomain_t* _d, char* _s, int _l)
{
	int res = 0, i;
	
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
	
	*_d = (pdomain_t*)shm_malloc(sizeof(pdomain_t));
	if (!(*_d)) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_pdomain(): No memory left\n");
		return -1;
	}
	memset(*_d, 0, sizeof(pdomain_t));
	
	(*_d)->table = (hslot_t*)shm_malloc(sizeof(hslot_t) * _s);
	if (!(*_d)->table) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_pdomain(): No memory left 2\n");
		shm_free(*_d);
		return -2;
	}

	(*_d)->name = _n;
	
	for(i = 0; i < _s; i++) {
		init_slot(*_d, &((*_d)->table[i]));
	}

	(*_d)->size = _s;
	init_lock((*_d)->lock);
	(*_d)->users = 0;
	(*_d)->expired = 0;
	
	(*_d)->reg = _r;
	(*_d)->unreg = _u;

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
		shm_free(_d->table);
	}
	unlock_pdomain(_d);

        shm_free(_d);
}


/*
 * Just for debugging
 */
void print_pdomain(FILE* _f, pdomain_t* _d)
{
	struct presentity* p;
	fprintf(_f, "---pdomain---\n");
	fprintf(_f, "name : \'%.*s\'\n", _d->name->len, _d->name->s);
	fprintf(_f, "size : %d\n", _d->size);
	fprintf(_f, "table: %p\n", _d->table);
	fprintf(_f, "first: %p\n", _d->first);
	fprintf(_f, "last : %p\n", _d->last);
	fprintf(_f, "lock : %d\n", _d->lock);
	if (_d->first) {
		fprintf(_f, "\n");
		p = _d->first;
		while(p) {
			print_presentity(_f, p);
			p = p->next;
		}
		fprintf(_f, "\n");
	}
	fprintf(_f, "---pdomain---\n");
}


int timer_pdomain(pdomain_t* _d)
{
	struct presentity* ptr, *t;

	lock_pdomain(_d);

	ptr = _d->first;

	while(ptr) {
		if (timer_presentity(ptr) < 0) {
			LOG(L_ERR, "timer_pdomain(): Error in timer_pdomain\n");
			unlock_pdomain(_d);
			return -1;
		}
		
		     /* Remove the entire record
		      * if it is empty
		      */
		if (ptr->watchers == 0) {
			t = ptr;
			ptr = ptr->next;
		} else {
			ptr = ptr->next;
		}
	}
	
	unlock_pdomain(_d);
	return 0;
}


/*
 * Get lock
 */
void lock_pdomain(pdomain_t* _d)
{
	get_lock(&_d->lock);
}


/*
 * Release lock
 */
void unlock_pdomain(pdomain_t* _d)
{
	release_lock(&_d->lock);
}


/*
 * Find a presentity in domain
 */
int find_presentity(pdomain_t* _d, str* _to, struct presentity** _p)
{
	int sl, i;
	struct presentity* p;
	
	sl = hash_func(_d, _to->s, _to->len);
	
	p = _d->table[sl].first;
	
	for(i = 0; i < _d->table[sl].n; i++) {
		if ((p->to.len == _to->len) && !memcmp(p->to.s, _to->s, _to->len)) {
			*_p = p;
			return 0;
		}
		
		p = p->next;
	}
	
	return 1;   /* Nothing found */
}


void add_presentity(pdomain_t* _d, struct presentity* _p)
{
	int sl;

	sl = hash_func(_d, _p->to.s, _p->to.len);

	slot_add(&_d->table[sl], _p, &_d->first, &_d->last);
}


void remove_presentity(pdomain_t* _d, struct presentity* _p)
{
	slot_rem(_p->slot, _p, &_d->first, &_d->last);
}
