/*
 * Presence Agent, watcher structure and related functions
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

#include "watcher.h"
#include "paerrno.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../trim.h"


/*
 * Create a new watcher structure
 */
int new_watcher(str* _from, str* _c, time_t _e, doctype_t _a, watcher_t** _w, str* callid, str* from_tag, str* to)
{
	*_w = (watcher_t*)shm_malloc(sizeof(watcher_t));
	if (!(*_w)) {
		paerrno = PA_NO_MEMORY;
	        LOG(L_ERR, "new_watcher(): No memory left\n");
		return -1;
	}

	(*_w)->from.s = (char*)shm_malloc(_from->len);
	if ((*_w)->from.s == 0) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_watcher(): No memory left 2\n");
		shm_free(*_w);
		return -2;
	}
	memcpy((*_w)->from.s, _from->s, _from->len);
	(*_w)->from.len = _from->len;

	(*_w)->contact.s = (char*)shm_malloc(_c->len);
	if ((*_w)->contact.s == 0) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_watcher(): No memory left\n");
		shm_free((*_w)->from.s);
		shm_free(*_w);
		return -3;
	}
	memcpy((*_w)->contact.s, _c->s, _c->len);
	(*_w)->contact.len = _c->len;

	(*_w)->expires = _e;
	(*_w)->next = 0;
	(*_w)->accept = _a;

	(*_w)->dialog.callid.s = (char*)shm_malloc(callid->len);
	memcpy((*_w)->dialog.callid.s, callid->s, callid->len);
	(*_w)->dialog.callid.len = callid->len;

	(*_w)->dialog.from_tag.s = (char*)shm_malloc(from_tag->len);
	memcpy((*_w)->dialog.from_tag.s, from_tag->s, from_tag->len);
	(*_w)->dialog.from_tag.len = from_tag->len;
	
	(*_w)->dialog.to.s = (char*)shm_malloc(to->len);
	memcpy((*_w)->dialog.to.s, to->s, to->len);
	(*_w)->dialog.to.len = to->len;

	(*_w)->dialog.cseq = 1;

	return 0;
}


/*
 * Release a watcher structure
 */
void free_watcher(watcher_t* _w)
{
	shm_free(_w->from.s);
	shm_free(_w->contact.s);

	shm_free(_w->dialog.callid.s);
	shm_free(_w->dialog.from_tag.s);

	shm_free(_w);	
}


/*
 * Print contact, for debugging purposes only
 */
void print_watcher(FILE* _f, watcher_t* _w)
{
	fprintf(_f, "~~~Watcher~~~\n");
	fprintf(_f, "from   : \'%.*s\'\n", _w->from.len, _w->from.s);
	fprintf(_f, "contact: \'%.*s\'\n", _w->contact.len, _w->contact.s);
	fprintf(_f, "expires: %d\n", (int)(_w->expires - time(0)));
	fprintf(_f, "accept : %s\n", (_w->accept == DOC_XPIDF) ? ("DOC_XPIDF") : ("DOC_LPIDF"));
	fprintf(_f, "next   : %p\n", _w->next);
	fprintf(_f, "dialog.callid: \'%.*s\'\n", _w->dialog.callid.len, _w->dialog.callid.s);
	fprintf(_f, "dialog.from_tag: \'%.*s\'\n", _w->dialog.from_tag.len, _w->dialog.from_tag.s);
	fprintf(_f, "dialog.to: \'%.*s\'\n", _w->dialog.to.len, _w->dialog.to.s);

	fprintf(_f, "~~~/Watcher~~~\n");
}


int update_watcher(watcher_t* _w, str* _c, time_t _e)
{
	if (_w->contact.s) {
		shm_free(_w->contact.s);
	}

	_w->contact.s = (char*)shm_malloc(_c->len);
	if (_w->contact.s == 0) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "update_watcher(): No memory left\n");
		return -1;
	}

	memcpy(_w->contact.s, _c->s, _c->len);
	_w->contact.len = _c->len;

	_w->expires = _e;

	return 0;
}
