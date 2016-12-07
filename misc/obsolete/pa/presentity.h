/*
 * Presence Agent, presentity structure and related functions
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2004 Jamey Hicks
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

#ifndef PRESENTITY_H
#define PRESENTITY_H

#include "../../str.h"
#include "../../modules/tm/dlg.h"
#include "watcher.h"
#include "hslot.h"
#include "trace.h"
#include "pdomain.h"

#include <xcap/pres_rules.h>
#include <cds/msg_queue.h>
#include <cds/list.h>
#include <presence/notifier.h>
#include <presence/subscriber.h>
#include <presence/pres_doc.h>
#include <cds/dbid.h>

typedef struct presence_tuple {
	presence_tuple_info_t data;
	/* Contact is constant for non-published tuples and it is allocated
	 * together with whole structure. For published tuples is contact 
	 * allocated separately in shared memory and can change. */
	
	int is_published;	/* 1 for published tuples - these are stored into DB */
	str etag;	/* etag for published tuples -> constant for tuple life */
	time_t expires; /* tuple expires on ... */
	str published_id;	/* tuple id used for publish -> constant for tuple life */
} presence_tuple_t;

typedef struct {
	presence_note_t data;
	
	str etag; /* published via this etag -> constant for note life */
	time_t expires; /* note expires on ... */
	str dbid; /* id for database ops - needed for removing expired notes -> constant for note life */
} pa_presence_note_t;

typedef struct _pa_extension_element_t {
	extension_element_t data;

	str etag; /* published via this etag -> constant for structure life */
	time_t expires; /* expires on ... */
	str dbid; /* id for database ops - needed for removing expired  -> constant for structure life */
} pa_extension_element_t;

typedef struct {
	str user;
	str contact;
	basic_tuple_status_t state;
} tuple_change_info_t;

struct pdomain;

typedef enum pflag {
	PFLAG_PRESENCE_CHANGED=1,
	PFLAG_WATCHERINFO_CHANGED=2
} pflag_t;

typedef struct _internal_pa_subscription_t {
	struct _internal_pa_subscription_t *prev, *next;
	watcher_status_t status;
	qsa_subscription_t *subscription;
	/* msg_queue_t *dst;
	 * str_t package; 
	 * str_t watcher_uri; */
} internal_pa_subscription_t;


typedef struct presentity {
	/* URI of presentity - doesn't change for the presentity's life */
	presentity_info_t data;
	str uuid; /* use after usrloc uuid-zation - callbacks are 
				 registered to this,  - doesn't change for 
				 the presentity's life  */
	
	str pres_id;   /* id of the record in the presentity table (generated!) */
	int id_cntr; /* variable for generating watcher/tuple/... ids */
	
	/* provisional data members */
	
	int ref_cnt;  /* reference counter - don't remove if > 1 */
	pflag_t flags;
	struct pdomain *pdomain; 
	struct presentity* next; /* Next presentity */
	struct presentity* prev; /* Previous presentity in list */
	struct hslot* slot;      /* Hash table collision slot we belong to */
	
	/* watchers/winfo watchers/internal watchers */
	
	watcher_t *first_watcher, *last_watcher;     /* List of watchers */
	watcher_t *first_winfo_watcher, *last_winfo_watcher;  /* Watchers subscribed to winfo */
	internal_pa_subscription_t *first_qsa_subscription, *last_qsa_subscription;
	
	/* authorization data */
	
	presence_rules_t *authorization_info;
	xcap_query_params_t xcap_params; /* doesn't change for the presentity's life (FIXME: rewrite) */
	time_t auth_rules_refresh_time;
	
	msg_queue_t mq;	/* message queue supplying direct usrloc callback processing */	

	/* data for internal subscriptions to presence 
	 * (reduces memory allocation count) */
	qsa_subscription_data_t presence_subscription_data;
	qsa_subscription_t *presence_subscription;
} presentity_t;


/* shortcuts for PA structures walking (PA uses derived structures
 * instead of that defined in presence library because it needs
 * to store more information) */

#define get_first_tuple(p)	((presence_tuple_t*)(p->data.first_tuple))
#define get_next_tuple(t)	((presence_tuple_t*)(t->data.next))

#define get_first_note(p)	((pa_presence_note_t*)(p->data.first_note))
#define get_next_note(n)	((pa_presence_note_t*)(n->data.next))

#define get_first_extension(p)	((pa_extension_element_t*)(p->data.first_unknown_element))
#define get_next_extension(pe)	((pa_extension_element_t*)(pe->data.next))

/** Create a new presentity. */
int new_presentity(struct pdomain *pdomain, str* _uri, str *uid, 
		xcap_query_params_t *xcap_params, presentity_t** _p);

/** Free all memory associated with a presentity - use only in special
 * cases like freeing memory on module cleanup. Otherwise use
 * release_presentity instead. */
void free_presentity(presentity_t* _p);

/* Free all memory associated with a presentity and remove it from DB */
void release_presentity(presentity_t* _p);

/** Removes all data for presentity (tuples, watchers, tuple notes, ...)
 * from given database table. 
 * It is possible due to that pres_id is unique identifier
 * common for all tables */
int db_remove_presentity_data(presentity_t* presentity, const char *table);

/* set authorization rules for presentity
 * ! call from locked region only ! */
int set_auth_rules(presentity_t *p, presence_rules_t *new_auth_rules);

/* Run a timer handler on the presentity - cleanup of expired data, sending
 * notifications when presentity modified, ... */
int timer_presentity(presentity_t* _p);

/********** UTILITY functions **********/

/* Gets UID from message (using get_to_uid) 
 * (it never allocates memory !!!) */
int get_presentity_uid(str *uid_dst, struct sip_msg *m);

/* 
 * converts uri to uid (uid is allocated in shm)
 * used by internal subscriptions and fifo commands 
 * FIXME: remove (internal subscriptions will be through UID too)
 */
int pres_uri2uid(str_t *uid_dst, const str_t *uri);

/* FIXME: change to local function within pdomain.c as soon as
 * will be message queue data types solved */
void free_tuple_change_info_content(tuple_change_info_t *i);

/************ Parameters **********/

/* how often refresh authorization rules (xcap change events are 
 * not implemented yet!) */
extern int auth_rules_refresh_time;

int pdomain_load_presentities(struct pdomain *pdomain);

#endif /* PRESENTITY_H */
