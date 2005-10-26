/*
 * Presence Agent, watcher structure and related functions
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef WATCHER_H
#define WATCHER_H

#include "../../str.h"
#include "../tm/dlg.h"
#include "../../db/db.h"
#include "../../parser/parse_content.h"
#include <stdio.h>
#include <time.h>

#define MIMETYPE(x_,y_) ((TYPE_##x_ << 16) | (SUBTYPE_##y_))

typedef enum doctype {
	DOC_XPIDF = MIMETYPE(APPLICATION,XPIDFXML),
	DOC_LPIDF = MIMETYPE(APPLICATION,LPIDFXML),
	DOC_PIDF =  MIMETYPE(APPLICATION,PIDFXML),
#ifdef SUBTYPE_XML_MSRTC_PIDF
	DOC_MSRTC_PIDF =  MIMETYPE(APPLICATION,XML_MSRTC_PIDF),
#endif
	DOC_WINFO = MIMETYPE(APPLICATION,WATCHERINFOXML),
//	DOC_XCAP_CHANGE = (1 << 4),
//	DOC_LOCATION = (1 << 5),
	DOC_MULTIPART_RELATED = MIMETYPE(MULTIPART,RELATED),
	DOC_RLMI_XML = MIMETYPE(APPLICATION,RLMIXML)
} doctype_t;

typedef enum watcher_status {
	WS_PENDING = 0,
	WS_ACTIVE = 1,
	WS_REJECTED = 2,
	WS_TERMINATED = 3,
	WS_PENDING_TERMINATED = 4
} watcher_status_t;

extern str watcher_status_names[];

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
	int preferred_mimetype; /* Type of document accepted by the watcher */
	int document_index;		/* many documents (winfo, ...) requires sequential numbering */
 	dlg_t* dialog;          /* Dialog handle */
	str s_id;               /* id of this watcherinfo statement */
	str server_contact;		/* used for contact header in NOTIFY messages */
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
int new_watcher_no_wb(struct presentity *_p, str* _uri, time_t _e, int event_package, doctype_t _a, dlg_t* _dlg, str *display_name, 
		str *server_contact, watcher_t** _w);

/* add watcher into db */
int db_add_watcher(struct presentity *_p, watcher_t *watcher);

/* update watcher in db */
int db_update_watcher(struct presentity *p, watcher_t* _w);

/* delete watcher from db */
int db_remove_watcher(struct presentity *_p, watcher_t *w);

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
int update_watcher(struct presentity *p, watcher_t* _w, time_t _e);

/*
 * Read watcherinfo table from database for presentity _p
 */
struct presentity;
int db_read_watcherinfo(struct presentity *_p, db_con_t* db);



/*
 * Add a watcher information to a winfo document
 */
int winfo_add_watcher(str* _b, int _l, watcher_t *watcher);

struct _internal_pa_subscription_t;
typedef struct _internal_pa_subscription_t internal_pa_subscription_t;

int winfo_add_internal_watcher(str* _b, int _l, internal_pa_subscription_t *iwatcher);
	
/*
 * Create start of winfo document
 */
int start_winfo_doc(str* _b, int _l, struct watcher *w);

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

/** Returns 1 if givn watcher is in one of terminated statuses 
 * and should be deleted */
int is_watcher_terminated(watcher_t *w);

/** Returns 1 if given watcher can receive status documents */
int is_watcher_authorized(watcher_t *w);

/** Sets status to correct terminated status for this watcher. */
void set_watcher_terminated_status(watcher_t *w);

int verify_event_package(int et);

#endif /* WATCHER_H */
