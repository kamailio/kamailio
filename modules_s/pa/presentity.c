/*
 * Presence Agent, presentity structure and related functions
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

#include "presentity.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "paerrno.h"
#include "notify.h"


/*
 * Create a new presentity
 */
int new_presentity(str* _to, presentity_t** _p)
{
	*_p = (presentity_t*)shm_malloc(sizeof(presentity_t));
	if (*_p == 0) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_presentity(): No memory left\n");
		return -1;
	}
	memset(*_p, 0, sizeof(presentity_t));

	(*_p)->to.s = (char*)shm_malloc(_to->len);
	if ((*_p)->to.s == 0) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_presentity(): No memory left 2\n");
		shm_free(*_p);
		return -2;
	}
	memcpy((*_p)->to.s, _to->s, _to->len);
	(*_p)->to.len = _to->len;
	return 0;
}



/*
 * Free all memory associated with a presentity
 */
int free_presentity(presentity_t* _p)
{
	watcher_t* ptr;

	while(_p->watchers) {
		ptr = _p->watchers;
		_p->watchers = _p->watchers->next;
		free_watcher(ptr);
	}
	
	if (_p->to.s) shm_free(_p->to.s);
	shm_free(_p);
}


/*
 * Print a presentity
 */
void print_presentity(FILE* _f, presentity_t* _p)
{
	watcher_t* ptr;

	fprintf(_f, "...Presentity...\n");
	fprintf(_f, "to: \'%.*s\'\n", _p->to.len, _p->to.s);
	
	if (_p->watchers) {
		ptr = _p->watchers;
		while(ptr) {
			print_watcher(_f, ptr);
			ptr = ptr->next;
		}
	}

	fprintf(_f, ".../Presentity...\n");
}


int timer_presentity(presentity_t* _p)
{

	
	return 0;
}



int add_watcher(presentity_t* _p, str* _from, str* _c, time_t _e, doctype_t _a, str* callid, str* from_tag, str* to, struct watcher** _w)
{
	if (new_watcher(_from, _c, _e, _a, _w, callid, from_tag, to) < 0) {
		LOG(L_ERR, "add_watcher(): Error while creating new watcher structure\n");
		return -1;
	}

	(*_w)->next = _p->watchers;
	_p->watchers = *_w;
}


int remove_watcher(presentity_t* _p, watcher_t* _w)
{
	watcher_t* ptr, *prev;

	ptr = _p->watchers;
	prev = 0;
	
	while(ptr) {
		if (ptr == _w) {
			if (prev) {
				prev->next = ptr->next;
			} else {
				_p->watchers = ptr->next;
			}
			return 0;
		}

		prev = ptr;
		ptr = ptr->next;
	}
	
	return 1;
}


int notify_watchers(presentity_t* _p)
{
	struct watcher* ptr;

	ptr = _p->watchers;

	while(ptr) {
		send_notify(_p, ptr);
		ptr = ptr->next;
	}
	return 0;
}
