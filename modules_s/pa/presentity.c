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


#include <stdio.h>
#include <string.h>
#include "../../db/db.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "paerrno.h"
#include "notify.h"
#include "presentity.h"
#include "ptime.h"

extern db_con_t* pa_db;
extern int use_db;
extern char *presentity_table;

str pstate_name[PS_NSTATES] = {
	{ "unknown", sizeof("unknown") - 1 },
	{ "online", sizeof("online") - 1 },
	{ "offline", sizeof("offline") - 1 },
	{ "away", sizeof("away") - 1 },
	{ "xaway", sizeof("xaway") - 1 },
	{ "dnd", sizeof("dnd") - 1 },
	{ "typing", sizeof("typing") - 1 },
};

int basic2status(str basic)
{
	int i;
	for ( i= 0; i < PS_NSTATES; i++ ) {
		if (strcmp(pstate_name[i].s, basic.s) == 0) {
			return i;
		}
	}
	return 0;
}

str str_strdup(str string)
{
	str new_string;
	new_string.s = shm_malloc(string.len + 1);
	new_string.len = string.len;
	strncpy(new_string.s, string.s, string.len);
	new_string.s[string.len] = 0;
	return new_string;
}

/*
 * Create a new presentity
 */
int new_presentity(str* _uri, presentity_t** _p)
{
	presentity_t* presentity;

	if (!_uri || !_p) {
		paerrno = PA_INTERNAL_ERROR;
		LOG(L_ERR, "new_presentity(): Invalid parameter value\n");
		return -1;
	}

	presentity = (presentity_t*)shm_malloc(sizeof(presentity_t) + _uri->len);
	if (!presentity) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_presentity(): No memory left\n");
		return -1;
	}
	memset(presentity, 0, sizeof(presentity_t));

	presentity->uri.s = (char*)presentity + sizeof(presentity_t);
	memcpy(presentity->uri.s, _uri->s, _uri->len);
	presentity->uri.len = _uri->len;
	*_p = presentity;
	
	if (use_db) {
		db_key_t query_cols[1];
		db_op_t  query_ops[1];
		db_val_t query_vals[1];

		db_key_t result_cols[4];
		db_res_t *res;
		int n_query_cols = 1;
		int n_result_cols = 0;
		int basic_col, status_col, location_col;

		LOG(L_INFO, "new_presentity: use_db starting\n");

		query_cols[0] = "uri";
		query_ops[0] = OP_EQ;
		query_vals[0].type = DB_STR;
		query_vals[0].nul = 0;
		query_vals[0].val.str_val = *_uri;

		result_cols[basic_col = n_result_cols++] = "basic";
		result_cols[status_col = n_result_cols++] = "status";
		result_cols[location_col = n_result_cols++] = "location";

		db_use_table(pa_db, presentity_table);
		if (db_query (pa_db, query_cols, query_ops, query_vals,
			      result_cols, n_query_cols, n_result_cols, 0, &res) < 0) {
			LOG(L_ERR, "db_new_presentity(): Error while querying presentity\n");
			return -1;
		}
		LOG(L_INFO, "new_presentity: getting values: res=%p res->n=%d\n",
		    res, (res ? res->n : 0));
		if (res && res->n > 0) {
			/* fill in presentity structure from database query result */
			db_row_t *row = &res->rows[0];
			db_val_t *row_vals = ROW_VALUES(row);
			str basic = row_vals[basic_col].val.str_val;
			// str status = row_vals[status_col].val.str_val;
			str location = row_vals[location_col].val.str_val;
			
			LOG(L_INFO, "  basic=%s location=%s\n", basic.s, location.s);

			presentity->state = basic2status(basic);
			presentity->location = str_strdup(location);

		} else {
			/* insert new record into database */
			LOG(L_INFO, "new_presentity: inserting into table\n");
			if (db_insert(pa_db, query_cols, query_vals, n_query_cols) < 0) {
				LOG(L_ERR, "db_new_presentity(): Error while inserting presentity\n");
				return -1;
			}
		}
		LOG(L_INFO, "new_presentity: use_db done\n");
	}

	return 0;
}


/*
 * Sync presentity to db if db is in use
 */
int db_update_presentity(presentity_t* _p)
{
	if (use_db) {
		db_key_t query_cols[1];
		db_op_t query_ops[1];
		db_val_t query_vals[1];
		int n_selectors = sizeof(query_cols)/sizeof(db_key_t);

		db_key_t update_cols[3];
		db_val_t update_vals[3];
		int n_updates = 1;

		LOG(L_INFO, "update_presentity starting\n");
		query_cols[0] = "uri";
		query_ops[0] = OP_EQ;
		query_vals[0].type = DB_STR;
		query_vals[0].nul = 0;
		query_vals[0].val.str_val = _p->uri;

		update_cols[0] = "basic";
		update_vals[0].type = DB_STR;
		update_vals[0].nul = 0;
		update_vals[0].val.str_val = pstate_name[_p->state];

		if (_p->location.len && _p->location.s) {
			update_cols[n_updates] = "location";
			update_vals[n_updates].type = DB_STR;
			update_vals[n_updates].nul = 0;
			update_vals[n_updates].val.str_val = _p->location;
			LOG(L_INFO, "  location=%s len=%d\n", _p->location.s, _p->location.len);
			
			n_updates++;
		}

		db_use_table(pa_db, presentity_table);

		LOG(L_INFO, "n_selectors=%d, n_updates=%d dbf.update=%p\n", 
		    n_selectors, n_updates, dbf.update);

		if (db_update(pa_db, 
			      query_cols, query_ops, query_vals, 
			      update_cols, update_vals, n_selectors, n_updates) < 0) {
			LOG(L_ERR, "db_update_presentity: Error while updating database\n");
			return -1;
		}

		LOG(L_INFO, "update_presentity done\n");
	}
	return 0;
}

/*
 * Free all memory associated with a presentity
 */
void free_presentity(presentity_t* _p)
{
	watcher_t* ptr;

	while(_p->watchers) {
		ptr = _p->watchers;
		_p->watchers = _p->watchers->next;
		free_watcher(ptr);
	}

	while(_p->winfo_watchers) {
		ptr = _p->winfo_watchers;
		_p->winfo_watchers = _p->winfo_watchers->next;
		free_watcher(ptr);
	}
	
	shm_free(_p);
}


/*
 * Print a presentity
 */
void print_presentity(FILE* _f, presentity_t* _p)
{
	watcher_t* ptr;

	fprintf(_f, "--presentity_t---\n");
	fprintf(_f, "uri: '%.*s'\n", _p->uri.len, ZSW(_p->uri.s));
	
	if (_p->watchers) {
		ptr = _p->watchers;
		while(ptr) {
			print_watcher(_f, ptr);
			ptr = ptr->next;
		}
	}

	if (_p->winfo_watchers) {
		ptr = _p->winfo_watchers;
		while(ptr) {
			print_watcher(_f, ptr);
			ptr = ptr->next;
		}
	}

	fprintf(_f, "---/presentity_t---\n");
}


int timer_presentity(presentity_t* _p)
{
	watcher_t* ptr, *t;

	ptr = _p->watchers;

	print_presentity(stdout, _p);
	while(ptr) {
	        if (ptr->expires <= act_time) {
		  LOG(L_ERR, "Removing watcher %.*s\n", ptr->uri.len, ptr->uri.s);
			ptr->expires = 0;
			send_notify(_p, ptr);
			t = ptr;
			ptr = ptr->next;
			remove_watcher(_p, t);
			free_watcher(t);
			continue;
		}
		
		ptr = ptr->next;
	}

	ptr = _p->winfo_watchers;

	print_presentity(stdout, _p);
	while(ptr) {
	        if (ptr->expires <= act_time) {
		  LOG(L_ERR, "Removing watcher %.*s\n", ptr->uri.len, ptr->uri.s);
			ptr->expires = 0;
			send_notify(_p, ptr);
			t = ptr;
			ptr = ptr->next;
			remove_winfo_watcher(_p, t);
			free_watcher(t);
			continue;
		}
		
		ptr = ptr->next;
	}
	return 0;
}


/*
 * Add a new watcher to the list
 */
int add_watcher(presentity_t* _p, str* _uri, time_t _e, doctype_t _a, dlg_t* _dlg, struct watcher** _w)
{
	if (new_watcher(_uri, _e, _a, _dlg, _w) < 0) {
		LOG(L_ERR, "add_watcher(): Error while creating new watcher structure\n");
		return -1;
	}

	(*_w)->next = _p->watchers;
	_p->watchers = *_w;
	return 0;
}


/*
 * Remove a watcher from the list
 */
int remove_watcher(presentity_t* _p, watcher_t* _w)
{
	watcher_t* ptr, *prev;

	ptr = _p->watchers;
	prev = 0;
	
	while(ptr) {
		if (ptr == _w) {
			if (prev) {
				prev->next = ptr->next;
			} else {
				_p->watchers = ptr->next;
			}
			return 0;
		}

		prev = ptr;
		ptr = ptr->next;
	}
	
	     /* Not found */
	DBG("remove_watcher(): Watcher not found in the list\n");
	return 1;
}


/*
 * Notify all watchers in the list
 */
int notify_watchers(presentity_t* _p)
{
	struct watcher* ptr;

	ptr = _p->watchers;

	while(ptr) {
		send_notify(_p, ptr);
		ptr = ptr->next;
	}
	return 0;
}

/*
 * Notify all winfo watchers in the list
 */
int notify_winfo_watchers(presentity_t* _p)
{
	struct watcher* ptr;

	ptr = _p->winfo_watchers;

	while(ptr) {
		send_notify(_p, ptr);
		ptr = ptr->next;
	}
	return 0;
}


/*
 * Add a new watcher to the winfo_watcher list
 */
int add_winfo_watcher(presentity_t* _p, str* _uri, time_t _e, doctype_t _a, dlg_t* _dlg, 
		      struct watcher** _w)
{
	if (new_watcher(_uri, _e, _a, _dlg, _w) < 0) {
		LOG(L_ERR, "add_winfo_watcher(): Error while creating new watcher structure\n");
		return -1;
	}

	(*_w)->next = _p->winfo_watchers;
	_p->winfo_watchers = *_w;
	return 0;
}


/*
 * Remove a watcher from the list
 */
int remove_winfo_watcher(presentity_t* _p, watcher_t* _w)
{
	watcher_t* ptr, *prev;

	ptr = _p->winfo_watchers;
	prev = 0;
	
	while(ptr) {
		if (ptr == _w) {
			if (prev) {
				prev->next = ptr->next;
			} else {
				_p->winfo_watchers = ptr->next;
			}
			return 0;
		}

		prev = ptr;
		ptr = ptr->next;
	}
	
	     /* Not found */
	DBG("remove_winfo_watcher(): Watcher not found in the list\n");
	return 1;
}

/*
 * Find a given watcher in the list
 */
int find_watcher(struct presentity* _p, str* _uri, watcher_t** _w)
{
	watcher_t* ptr;

	/* first look for watchers */
	ptr = _p->watchers;

	while(ptr) {
		if ((_uri->len == ptr->uri.len) &&
		    (!memcmp(_uri->s, ptr->uri.s, _uri->len))) {

			*_w = ptr;
			return 0;
		}
			
		ptr = ptr->next;
	}

	/* now look for winfo watchers */
	ptr = _p->winfo_watchers;

	while(ptr) {
		if ((_uri->len == ptr->uri.len) &&
		    (!memcmp(_uri->s, ptr->uri.s, _uri->len))) {

			*_w = ptr;
			return 0;
		}
			
		ptr = ptr->next;
	}
	
	return 1;
}
