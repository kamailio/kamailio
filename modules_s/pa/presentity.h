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


#define TUPLE_LOCATION_STR_LEN (128 + 32 + 32 + 64 + 32)
#define TUPLE_LOCATION_LOC_OFFSET 0
#define TUPLE_LOCATION_SITE_OFFSET 128
#define TUPLE_LOCATION_FLOOR_OFFSET (128+32)
#define TUPLE_LOCATION_ROOM_OFFSET (128+32+32)
#define TUPLE_LOCATION_PACKET_LOSS_OFFSET (128+32+32+64)

typedef struct location {
	str   loc; /* human readable description of location */
	str   site;
	str   floor;
	str   room;
	str   packet_loss;
	double x;
	double y;
	double radius;
} location_t;

typedef struct resource_list {
	str   uri;
	struct resource_list *next;
	struct resource_list *prev;
} resource_list_t;

typedef struct location_package {
	resource_list_t *users;
	resource_list_t *phones;
} location_package_t;

typedef struct presence_tuple {
	str contact;
	pstate_t state;
	location_t location;
	struct presence_tuple *next;
	struct presence_tuple *prev;
} presence_tuple_t;

struct pdomain;

typedef enum pflag {
	PFLAG_PRESENCE_CHANGED=1,
	PFLAG_PRESENCE_LISTS_CHANGED=2,
	PFLAG_WATCHERINFO_CHANGED=4,
	PFLAG_XCAP_CHANGED=8,
	PFLAG_LOCATION_CHANGED=16
} pflag_t;

typedef struct presentity {
	str uri;                 /* URI of presentity */
	int event_package;       /* parsed event package */
	presence_tuple_t *tuples;
	location_package_t location_package;
	watcher_t* watchers;     /* List of watchers */
	watcher_t* winfo_watchers;  /* Watchers subscribed to winfo */
	pflag_t flags;
	struct pdomain *pdomain; 
	struct presentity* next; /* Next presentity */
	struct presentity* prev; /* Previous presentity in list */
	struct hslot* slot;      /* Hash table collision slot we belong to */
} presentity_t;

/*
 * Create a new presentity
 */
int new_presentity(struct pdomain *pdomain, str* _uri, int event_package, presentity_t** _p);


/*
 * Free all memory associated with a presentity
 */
void free_presentity(presentity_t* _p);

/*
 * Sync presentity to db if db is in use
 */
int db_update_presentity(presentity_t* _p);

/*
 * Run a timer handler on the presentity
 */
int timer_presentity(presentity_t* _p);


/*
 * Create a new presence_tuple
 */
int new_presence_tuple(str* _contact, presentity_t *_p, presence_tuple_t ** _t);

/*
 * Find a presence_tuple for contact _contact on presentity _p
 */
int find_presence_tuple(str* _contact, presentity_t *_p, presence_tuple_t ** _t);
void add_presence_tuple(presentity_t *_p, presence_tuple_t *_t);
void remove_presence_tuple(presentity_t *_p, presence_tuple_t *_t);

/*
 * Free all memory associated with a presence_tuple
 */
void free_presence_tuple(presence_tuple_t * _t);



/*
 * Add a watcher to the watcher list
 */
int add_watcher(presentity_t* _p, str* _uri, time_t _e, int event_type, doctype_t _a, dlg_t* _dlg, 
		str *_dn, struct watcher** _w);


/*
 * Remove a watcher from the watcher list
 */
int remove_watcher(presentity_t* _p, watcher_t* _w);


/*
 * Find a watcher on the watcher list
 */
int find_watcher(presentity_t* _p, str* _uri, int etc, struct watcher** _w);

/*
 * Notify all watchers on the list
 */
int notify_watchers(presentity_t* _p);


/*
 * Add a watcher to the winfo watcher list
 */
int add_winfo_watcher(presentity_t* _p, str* _uri, time_t _e, int event_type, doctype_t _a, dlg_t* _dlg, 
		      str *_dn, struct watcher** _w);


/*
 * Remove a watcher from the winfo watcher list
 */
int remove_winfo_watcher(presentity_t* _p, watcher_t* _w);

/*
 * Notify all winfo watchers in the list
 */
int notify_winfo_watchers(presentity_t* _p);

/*
 * Print a presentity, just for debugging
 */
void print_presentity(FILE* _f, presentity_t* _p);


resource_list_t *resource_list_append_unique(resource_list_t *list, str *uri);
resource_list_t *resource_list_remove(resource_list_t *list, str *uri);

#endif /* PRESENTITY_H */

