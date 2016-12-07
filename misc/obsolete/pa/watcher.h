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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef WATCHER_H
#define WATCHER_H

#include "../../str.h"
#include "../../modules/tm/dlg.h"
#include "../../lib/srdb2/db.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_event.h" /* EVENT_PRESENCE, EVENT_PRESENCE_WINFO, ... */
#include <stdio.h>
#include <time.h>

#define DOC_XPIDF        MIMETYPE(APPLICATION,XPIDFXML)
#define DOC_LPIDF        MIMETYPE(APPLICATION,LPIDFXML)
#define DOC_PIDF         MIMETYPE(APPLICATION,PIDFXML)
#define DOC_CPIM_PIDF    MIMETYPE(APPLICATION,CPIM_PIDFXML)
#define DOC_MSRTC_PIDF   MIMETYPE(APPLICATION,XML_MSRTC_PIDF)
#define DOC_WINFO        MIMETYPE(APPLICATION,WATCHERINFOXML)
/*	DOC_XCAP_CHANGE = (1 << 4),
	DOC_LOCATION = (1 << 5),*/
#define DOC_MULTIPART_RELATED MIMETYPE(MULTIPART,RELATED),
#define DOC_RLMI_XML          MIMETYPE(APPLICATION,RLMIXML)

typedef enum watcher_status {
	WS_PENDING = 0,
	WS_ACTIVE = 1,
	WS_REJECTED = 2,
	WS_TERMINATED = 3,
	WS_PENDING_TERMINATED = 4
} watcher_status_t;

extern str watcher_status_names[];
extern str watcher_event_names[];

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
	str id;                 /* id of this watcher (used for DB and winfo docs) */
	str server_contact;		/* used for contact header in NOTIFY messages */
	wflags_t flags;
	watcher_event_t  event;
	watcher_status_t status; /* status of subscription */
	struct watcher *prev, *next;   /* linking members */
} watcher_t;

struct presentity;

/* Convert watcher status name to enum */
watcher_status_t watcher_status_from_string(str *wsname);

/* Create a new watcher structure */
int new_watcher_no_wb(str* _uri, time_t _e, int event_package, 
		int doc_type, dlg_t* _dlg, str *display_name, 
		str *server_contact, 
		str *id, /* database ID or NULL if not loading from DB */
		watcher_t** _w);

/* Release a watcher structure */
void free_watcher(watcher_t* _w);

/** Appends watcher/winfo watcher to presentity. It updates presentity's
 * flags and adds the watcher into DB if requested and use_db set. */
int append_watcher(struct presentity *_p, watcher_t *_w, int add_to_db);
	
/* Remove a watcher/winfo watcher from the watcher list and from database. */
void remove_watcher(struct presentity* _p, watcher_t* _w);

/* Find a watcher/winfo watcher in the list/winfo list (according to
 * _et parameter) via dialog identifier */
int find_watcher_dlg(struct presentity* _p, dlg_id_t *dlg_id, int _et, watcher_t** _w);

/* update watcher in db */
int db_update_watcher(struct presentity *p, watcher_t* _w);

/* Update expires value of a watcher */
int update_watcher(struct presentity *p, watcher_t* _w, time_t _e, struct sip_msg *m);

/* Read watcherinfo table from database for presentity _p */
int db_read_watcherinfo(struct presentity *_p, db_con_t* db);

/** Returns 1 if given watcher is in one of terminated statuses 
 * and should be deleted */
int is_watcher_terminated(watcher_t *w);

/** Returns 1 if given watcher can receive status documents */
int is_watcher_authorized(watcher_t *w);

/** Sets status to correct terminated status for this watcher. */
void set_watcher_terminated_status(watcher_t *w);

const char *event_package2str(int et);

#endif /* WATCHER_H */
