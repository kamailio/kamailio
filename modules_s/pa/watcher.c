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

#include "paerrno.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../trim.h"
#include "../../ut.h"
#include "pa_mod.h"
#include "watcher.h"


/*
 * Create a new watcher structure
 */
int new_watcher(str* _uri, time_t _e, doctype_t _a, dlg_t* _dlg, watcher_t** _w)
{
	watcher_t* ptr;

	     /* Check parameters */
	if (!_uri && !_dlg && !_w) {
		LOG(L_ERR, "new_watcher(): Invalid parameter value\n");
		return -1;
	}

	     /* Allocate memory buffer for watcher_t structure and uri string */
	ptr = (watcher_t*)shm_malloc(sizeof(watcher_t) + _uri->len);
	if (!ptr) {
		paerrno = PA_NO_MEMORY;
	        LOG(L_ERR, "new_watcher(): No memory left\n");
		return -1;
	}
	memset(ptr, 0, sizeof(watcher_t));

	     /* Copy uri string */
	ptr->uri.s = (char*)ptr + sizeof(watcher_t);
	memcpy(ptr->uri.s, _uri->s, _uri->len);
	
	ptr->expires = _e; /* Expires value */
	ptr->accept = _a;  /* Accepted document type */
	ptr->dialog = _dlg; /* Dialog handle */
	*_w = ptr;
	return 0;
}


/*
 * Release a watcher structure
 */
void free_watcher(watcher_t* _w)
{
	tmb.free_dlg(_w->dialog);
	shm_free(_w);	
}


/*
 * Print contact, for debugging purposes only
 */
void print_watcher(FILE* _f, watcher_t* _w)
{
	fprintf(_f, "---Watcher---\n");
	fprintf(_f, "uri    : '%.*s'\n", _w->uri.len, ZSW(_w->uri.s));
	fprintf(_f, "expires: %d\n", (int)(_w->expires - time(0)));
	fprintf(_f, "accept : %s\n", (_w->accept == DOC_XPIDF) ? ("DOC_XPIDF") : ("DOC_LPIDF"));
	fprintf(_f, "next   : %p\n", _w->next);
	tmb.print_dlg(_f, _w->dialog);
	fprintf(_f, "---/Watcher---\n");
}


/*
 * Update a watcher structure
 */
int update_watcher(watcher_t* _w, time_t _e)
{
	_w->expires = _e;
	return 0;
}
