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

#ifndef WATCHER_H
#define WATCHER_H

#include "../../str.h"
#include "../tm/dlg.h"
#include <stdio.h>
#include <time.h>


typedef enum doctype {
	DOC_XPIDF = 0,
	DOC_LPIDF
} doctype_t;


/*
 * Structure representing a watcher
 */
typedef struct watcher {
	str uri;                /* Uri of the watcher */
	time_t expires;         /* Absolute expiry time */
	doctype_t accept;       /* Type of document accepted by the watcher */
	dlg_t* dialog;          /* Dialog handle */
	struct watcher* next;   /* Next watcher in the list */
} watcher_t;
 

/*
 * Create a new watcher structure
 */
int new_watcher(str* _uri, time_t _e, doctype_t _a, dlg_t* _dlg, watcher_t** _w);


/*
 * Release a watcher structure
 */
void free_watcher(watcher_t* _w);


/*
 * Print contact, for debugging purposes only
 */
void print_watcher(FILE* _f, watcher_t* _w);


/*
 * Update expires value of a watcher
 */
int update_watcher(watcher_t* _w, time_t _e);


#endif /* WATCHER_H */
