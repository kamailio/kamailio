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

#ifndef PRESENTITY_H
#define PRESENTITY_H


#include "../../str.h"
#include "../tm/dlg.h"
#include "watcher.h"
#include "hslot.h"
#include "pstate.h"


typedef struct presentity {
	str uri;                 /* URI of presentity */
	pstate_t state;          /* State of presentity */
	watcher_t* watchers;     /* List of watchers */
	struct presentity* next; /* Next presentity */
	struct presentity* prev; /* Previous presentity in list */
	struct hslot* slot;      /* Hash table collision slot we belong to */
} presentity_t;


/*
 * Create a new presentity
 */
int new_presentity(str* _uri, presentity_t** _p);


/*
 * Free all memory associated with a presentity
 */
void free_presentity(presentity_t* _p);


/*
 * Run a timer handler on the presentity
 */
int timer_presentity(presentity_t* _p);


/*
 * Add a watcher to the watcher list
 */
int add_watcher(presentity_t* _p, str* _uri, time_t _e, doctype_t _a, dlg_t* _dlg, struct watcher** _w);


/*
 * Remove a watcher from the watcher list
 */
int remove_watcher(presentity_t* _p, watcher_t* _w);


/*
 * Notify all watchers on the list
 */
int notify_watchers(presentity_t* _p);


/*
 * Print a presentity, just for debugging
 */
void print_presentity(FILE* _f, presentity_t* _p);


#endif /* PRESENTITY_H */

