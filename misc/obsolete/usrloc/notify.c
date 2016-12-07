/*
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
 *
 */

#include "notify.h"
#include "../../ut.h"
#include "../../mem/shm_mem.h"
#include "dlist.h"
#include "udomain.h"


str dom = STR_STATIC_INIT("location");


void notify_watchers(struct urecord* _r, ucontact_t *_c, int state)
{
	notify_cb_t* n;
       
	n = _r->watchers;
        while(n) {
		n->cb(&_r->uid, &_c->c, state, n->data);
		n = n->next;
	}
}


int add_watcher(struct urecord* _r, notcb_t _c, void* _d)
{
	notify_cb_t* ptr;
	ucontact_t *c;

	ptr = (notify_cb_t*)shm_malloc(sizeof(notify_cb_t));
	if (ptr == 0) {
		LOG(L_ERR, "add_watcher(): No memory left\n");
		return -1;
	}

	ptr->cb = _c;
	ptr->data = _d;
	ptr->next = _r->watchers;
	_r->watchers = ptr;

	c = _r->contacts;
	while (c) {
		ptr->cb(&_r->uid, &c->c, PRES_ONLINE, ptr->data);
		c = c->next;
	}
	return 0;
}


int remove_watcher(struct urecord* _r, notcb_t _c, void* _d)
{
	notify_cb_t* ptr, *prev = 0;;

	ptr = _r->watchers;
	while(ptr) {
		if ((ptr->cb == _c) && (ptr->data == _d)) {
			if (prev) prev->next = ptr->next;
			else _r->watchers = ptr->next;
			shm_free(ptr);
			return 0;
		}
		prev = ptr;
		ptr = ptr->next;
	}
	return 1;
}


int register_watcher(str* _f, str* _t, notcb_t _c, void* _data)
{
	udomain_t* d;
	urecord_t* r;

	if (find_domain(&dom, &d) > 0) {
		LOG(L_ERR, "register_watcher(): Domain '%.*s' not found\n", dom.len, ZSW(dom.s));
		return -1;
	}

	lock_udomain(d);

	if (get_urecord(d, _t, &r) > 0) {
		if (insert_urecord(d, _t, &r) < 0) {
			unlock_udomain(d);
			LOG(L_ERR, "register_watcher(): Error while creating a new record\n");
			return -2;
		}
	}

	if (add_watcher(r, _c, _data) < 0) {
		LOG(L_ERR, "register_watcher(): Error while adding a watcher\n");
		release_urecord(r);
		unlock_udomain(d);
		return -3;
	}

	unlock_udomain(d);

	return 0;
}


int unregister_watcher(str* _f, str* _t, notcb_t _c, void* _data)
{
	udomain_t* d;
	urecord_t* r;

	if (find_domain(&dom, &d) > 0) {
		LOG(L_ERR, "unregister_watcher(): Domain '%.*s' not found\n", dom.len, ZSW(dom.s));
		return -1;
	}
	
	lock_udomain(d);
	
	if (get_urecord(d, _t, &r) > 0) {
		unlock_udomain(d);
		DBG("unregister_watcher(): Record not found\n");
		return 0;
	}

	remove_watcher(r, _c, _data);
	release_urecord(r);

	unlock_udomain(d);

	return 0;
}
