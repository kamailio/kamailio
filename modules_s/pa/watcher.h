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
	DOC_LPIDF = 1,
	DOC_PIDF = 2,
	DOC_WINFO = 3,
	DOC_XCAP_CHANGE = 4,
	DOC_LOCATION =5
} doctype_t;


typedef enum watcher_status {
	WS_PENDING = 0,
	WS_ACTIVE = 1,
	WS_WAITING = 2,
	WS_TERMINATED = 3
} watcher_status_t;

typedef enum watcher_event {
	WE_SUBSCRIBE = 0,
	WE_APPROVED = 1,
	WE_DEACTIVATED = 2,
	WE_PROBATION = 3,
	WE_REJECTED = 4,
	WE_TIMEOUT = 5,
	WE_GIVEUP = 6,
	WE_NORESOURCE = 7
} watcher_event_t;

typedef enum wflags {
	WFLAG_SUBSCRIPTION_CHANGED=1
} wflags_t;

/*
 * Structure representing a watcher
 */
typedef struct watcher {
	str display_name;       /* Display Name of watcher */
	str uri;                /* Uri of the watcher */
	time_t expires;         /* Absolute of the expiration */
	int event_package;      /* event package being watched */
	doctype_t accept;       /* Type of document accepted by the watcher */
	dlg_t* dialog;          /* Dialog handle */
	str s_id;               /* id of this watcherinfo statement */
	wflags_t flags;
        watcher_event_t  event;
	watcher_status_t status; /* status of subscription */
	struct watcher* next;   /* Next watcher in the list */
} watcher_t;
 

/*
 * Convert watcher status name to enum
 */
watcher_status_t watcher_status_from_string(str *wsname);

/*
 * Convert watcher event name to enum
 */
watcher_event_t watcher_event_from_string(str *wename);

/*
 * Create a new watcher structure
 */
struct presentity;
int new_watcher(struct presentity *_p, str* _uri, time_t _e, int event_package, doctype_t _a, dlg_t* _dlg, str *display_name, 
		watcher_t** _w);


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

/*
 * Read watcherinfo table from database for presentity _p
 */
struct presentity;
int db_read_watcherinfo(struct presentity *_p);



/*
 * Add a watcher information to a winfo document
 */
int winfo_add_watcher(str* _b, int _l, watcher_t *watcher);

/*
 * Create start of winfo document
 */
int start_winfo_doc(str* _b, int _l);

/*
 * Start a resource in a winfo document
 */
int winfo_start_resource(str* _b, int _l, str* _uri, watcher_t *watcher);
/*
 * End a resource in a winfo document
 */
int winfo_end_resource(str *_b, int _l);

/*
 * End a winfo document
 */
int end_winfo_doc(str* _b, int _l);

#endif /* WATCHER_H */
