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


#include <stdio.h>
#include <string.h>
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "paerrno.h"
#include "notify.h"
#include "presentity.h"
#include "ptime.h"


/*
 * Create a new presentity
 */
int new_presentity(str* _uri, presentity_t** _p)
{
	presentity_t* ptr;

	if (!_uri || !_p) {
		paerrno = PA_INTERNAL_ERROR;
		LOG(L_ERR, "new_presentity(): Invalid parameter value\n");
		return -1;
	}

	ptr = (presentity_t*)shm_malloc(sizeof(presentity_t) + _uri->len);
	if (!ptr) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_presentity(): No memory left\n");
		return -1;
	}
	memset(ptr, 0, sizeof(presentity_t));

	ptr->uri.s = (char*)ptr + sizeof(presentity_t);
	memcpy(ptr->uri.s, _uri->s, _uri->len);
	ptr->uri.len = _uri->len;
	*_p = ptr;
	return 0;
}


/*
 * Free all memory associated with a presentity
 */
void free_presentity(presentity_t* _p)
{
	watcher_t* ptr;

	while(_p->watchers) {
		ptr = _p->watchers;
		_p->watchers = _p->watchers->next;
		free_watcher(ptr);
	}
	
	shm_free(_p);
}


/*
 * Print a presentity
 */
void print_presentity(FILE* _f, presentity_t* _p)
{
	watcher_t* ptr;

	fprintf(_f, "--presentity_t---\n");
	fprintf(_f, "uri: '%.*s'\n", _p->uri.len, ZSW(_p->uri.s));
	
	if (_p->watchers) {
		ptr = _p->watchers;
		while(ptr) {
			print_watcher(_f, ptr);
			ptr = ptr->next;
		}
	}

	fprintf(_f, "---/presentity_t---\n");
}


int timer_presentity(presentity_t* _p)
{
	watcher_t* ptr, *t;

	ptr = _p->watchers;

	print_presentity(stdout, _p);
	while(ptr) {
	        if (ptr->expires <= act_time) {
		  LOG(L_ERR, "Removing watcher %.*s\n", ptr->uri.len, ptr->uri.s);
			ptr->expires = 0;
			send_notify(_p, ptr);
			t = ptr;
			ptr = ptr->next;
			remove_watcher(_p, t);
			free_watcher(t);
			continue;
		}
		
		ptr = ptr->next;
	}
	return 0;
}


/*
 * Add a new watcher to the list
 */
int add_watcher(presentity_t* _p, str* _uri, time_t _e, doctype_t _a, dlg_t* _dlg, struct watcher** _w)
{
	if (new_watcher(_uri, _e, _a, _dlg, _w) < 0) {
		LOG(L_ERR, "add_watcher(): Error while creating new watcher structure\n");
		return -1;
	}

	(*_w)->next = _p->watchers;
	_p->watchers = *_w;
	return 0;
}


/*
 * Remove a watcher from the list
 */
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
	
	     /* Not found */
	DBG("remove_watcher(): Watcher not found in the list\n");
	return 1;
}


/*
 * Notify all watchers in the list
 */
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


/*
 * Find a given watcher in the list
 */
int find_watcher(struct presentity* _p, str* _uri, watcher_t** _w)
{
	watcher_t* ptr;

	ptr = _p->watchers;

	while(ptr) {
		if ((_uri->len == ptr->uri.len) &&
		    (!memcmp(_uri->s, ptr->uri.s, _uri->len))) {

			*_w = ptr;
			return 0;
		}
			
		ptr = ptr->next;
	}
	
	return 1;
}
