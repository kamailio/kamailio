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
#include "../../parser/parse_event.h"
#include "paerrno.h"
#include "dlist.h"
#include "notify.h"
#include "pdomain.h"
#include "presentity.h"
#include "ptime.h"
#include "pa_mod.h"
#include "location.h"

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
		if (str_strcasecmp(&pstate_name[i], &basic) == 0) {
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
 * Create a new presentity but do not update database
 */
int new_presentity_no_wb(struct pdomain *pdomain, str* _uri, presentity_t** _p)
{
     presentity_t* presentity;
     int size = 0;

     if (!_uri || !_p) {
	  paerrno = PA_INTERNAL_ERROR;
	  LOG(L_ERR, "new_presentity(): Invalid parameter value\n");
	  return -1;
     }

     size = sizeof(presentity_t) + _uri->len + 1;
     presentity = (presentity_t*)shm_malloc(size);
     if (!presentity) {
	  paerrno = PA_NO_MEMORY;
	  LOG(L_ERR, "new_presentity(): No memory left: size=%d\n", size);
	  return -1;
     }
     memset(presentity, 0, sizeof(presentity_t));


     presentity->uri.s = ((char*)presentity) + sizeof(presentity_t);
     strncpy(presentity->uri.s, _uri->s, _uri->len);
     presentity->uri.s[_uri->len] = 0;
     presentity->uri.len = _uri->len;
     presentity->pdomain = pdomain;

     *_p = presentity;

     LOG(L_ERR, "new_presentity_no_wb=%p for uri=%.*s\n", 
	 presentity, presentity->uri.len, presentity->uri.s);

     return 0;
}

/*
 * Create a new presentity
 */
int new_presentity(struct pdomain *pdomain, str* _uri, presentity_t** _p)
{
     presentity_t* presentity;
     int size = 0;

     if (!_uri || !_p) {
	  paerrno = PA_INTERNAL_ERROR;
	  LOG(L_ERR, "new_presentity(): Invalid parameter value\n");
	  return -1;
     }

     size = sizeof(presentity_t) + _uri->len + 1;
     presentity = (presentity_t*)shm_malloc(size);
     if (!presentity) {
	  paerrno = PA_NO_MEMORY;
	  LOG(L_ERR, "new_presentity(): No memory left: size=%d\n", size);
	  return -1;
     }
     memset(presentity, 0, sizeof(presentity_t));

     presentity->uri.s = ((char*)presentity) + sizeof(presentity_t);
     strncpy(presentity->uri.s, _uri->s, _uri->len);
     presentity->uri.s[_uri->len] = 0;
     presentity->uri.len = _uri->len;
     presentity->pdomain = pdomain;

     if (use_db) {
	  db_key_t query_cols[4];
	  db_op_t  query_ops[4];
	  db_val_t query_vals[4];

	  db_key_t result_cols[4];
	  db_res_t *res;
	  int n_query_cols = 0;
	  int n_result_cols = 0;
	  int presid_col;
	  int presid = 0;

	  query_cols[0] = "uri";
	  query_ops[0] = OP_EQ;
	  query_vals[0].type = DB_STR;
	  query_vals[0].nul = 0;
	  query_vals[0].val.str_val = presentity->uri;
	  n_query_cols++;

	  query_cols[n_query_cols] = "pdomain";
	  query_ops[n_query_cols] = OP_EQ;
	  query_vals[n_query_cols].type = DB_STR;
	  query_vals[n_query_cols].nul = 0;
	  query_vals[n_query_cols].val.str_val = *presentity->pdomain->name;
	  n_query_cols++;

	  result_cols[presid_col = n_result_cols++] = "presid";

	  if (pa_dbf.use_table(pa_db, presentity_table) < 0) {
		  LOG(L_ERR, "new_presentity: Error in use_table\n");
		  return -1;
	  }

	  while (!presid) {
	       if (pa_dbf.query (pa_db, query_cols, query_ops, query_vals,
			     result_cols, n_query_cols, n_result_cols, 0, &res) < 0) {
		    LOG(L_ERR, "new_presentity: Error while querying presentity\n");
		    return -1;
	       }
	       if (res && res->n > 0) {
		    /* fill in tuple structure from database query result */
		    db_row_t *row = &res->rows[0];
		    db_val_t *row_vals = ROW_VALUES(row);
		    presid = presentity->presid = row_vals[presid_col].val.int_val;

		    LOG(L_INFO, "  presid=%d\n", presid);

	       } else {
		    /* insert new record into database */
		    LOG(L_INFO, "new_presentity: inserting %d cols into table\n", n_query_cols);
		    if (pa_dbf.insert(pa_db, query_cols, query_vals, n_query_cols)
					< 0) {
			 LOG(L_ERR, "new_presentity: Error while inserting tuple\n");
			 return -1;
		    }
	       }
	       pa_dbf.free_result(pa_db, res);
	  }
     }

     *_p = presentity;

     LOG(L_ERR, "new_presentity=%p for uri=%.*s\n", 
	 presentity, presentity->uri.len, presentity->uri.s);

     return 0;
}



/*
 * Free all memory associated with a presentity
 */
void free_presentity(presentity_t* _p)
{
	watcher_t* ptr;
	presence_tuple_t *tuple;

	return;
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
	
	while(_p->tuples) {
		tuple = _p->tuples;
		_p->tuples = _p->tuples->next;
		free_presence_tuple(tuple);
	}

	shm_free(_p);
}


/*
 * Sync presentity to db if db is in use
 */
int db_update_presentity(presentity_t* _p)
{
     if (use_db) {
	  presence_tuple_t *tuple;
	  db_key_t query_cols[22];
	  db_op_t query_ops[22];
	  db_val_t query_vals[22];
	  int n_selectors = 2;

	  int n_updates = 2;
	  int presid = _p->presid;

	  for (tuple = _p->tuples; tuple; tuple = tuple->next) {

	       n_selectors = 2;
	       n_updates = 2;

	       LOG(L_ERR, "db_update_presentity starting: use_place_table=%d presid=%d\n", use_place_table, presid);
	       query_cols[0] = "presid";
	       query_ops[0] = OP_EQ;
	       query_vals[0].type = DB_INT;
	       query_vals[0].nul = 0;
	       query_vals[0].val.int_val = presid;

	       query_cols[1] = "contact";
	       query_ops[1] = OP_EQ;
	       query_vals[1].type = DB_STR;
	       query_vals[1].nul = 0;
	       query_vals[1].val.str_val.s = tuple->contact.s;
	       query_vals[1].val.str_val.len = tuple->contact.len;
	       LOG(L_ERR, "db_update_presentity:  tuple->contact=%.*s len=%d\n basic=%d expires=%ld priority=%f", 
		   tuple->contact.len, tuple->contact.s, tuple->contact.len, tuple->state,
		   tuple->expires, tuple->priority);

	       {
		    int n_query_cols = 2;
		    LOG(L_INFO, "db_update_presentity: cleaning contact from table\n");
		    if (pa_dbf.use_table(pa_db, presentity_contact_table) < 0) {
			    LOG(L_ERR, "db_update_presentity: Error in use_table\n");
			    return -1;
		    }
		    if (pa_dbf.delete(pa_db, query_cols, query_ops, query_vals,
				      n_query_cols) < 0) {
			 LOG(L_ERR, "db_update_presentity: Error while deleting tuple\n");
			 return -1;
		    }
		    if (tuple->state == PS_OFFLINE)
			 continue;
	       }

	       query_cols[n_updates] = "basic";
	       query_vals[n_updates].type = DB_STR;
	       query_vals[n_updates].nul = 0;
	       query_vals[n_updates].val.str_val.s = pstate_name[tuple->state].s;
	       query_vals[n_updates].val.str_val.len = strlen(pstate_name[tuple->state].s);
	       n_updates++;

	       if (use_place_table) {
		    int placeid = 0;
		    LOG(L_ERR, "db_update_presentity: room=%.*s loc=%.*s\n", 
			tuple->location.room.len, tuple->location.room.s,
			tuple->location.loc.len, tuple->location.loc.s);
		    
		    if (tuple->location.room.len && tuple->location.room.s) {
			 location_lookup_placeid(&tuple->location.room, &placeid);
		    } else if (tuple->location.loc.len && tuple->location.loc.s) {
			 location_lookup_placeid(&tuple->location.loc, &placeid);
		    }
		    if (placeid) {
			 query_cols[n_updates] = "placeid";
			 query_vals[n_updates].type = DB_INT;
			 query_vals[n_updates].nul = 0;
			 query_vals[n_updates].val.int_val = placeid;
			 n_updates++;
		    }
	       } else {
		    if (tuple->location.loc.len && tuple->location.loc.s) {
			 query_cols[n_updates] = "location";
			 query_vals[n_updates].type = DB_STR;
			 query_vals[n_updates].nul = 0;
			 query_vals[n_updates].val.str_val = tuple->location.loc;
			 LOG(L_ERR, "db_update_presentity:  tuple->location.loc=%s len=%d\n", 
			     tuple->location.loc.s, tuple->location.loc.len);
			 n_updates++;
		    }
		    if (tuple->location.site.len && tuple->location.site.s) {
			 query_cols[n_updates] = "site";
			 query_vals[n_updates].type = DB_STR;
			 query_vals[n_updates].nul = 0;
			 query_vals[n_updates].val.str_val = tuple->location.site;
			 n_updates++;
		    }
		    if (tuple->location.floor.len && tuple->location.floor.s) {
			 query_cols[n_updates] = "floor";
			 query_vals[n_updates].type = DB_STR;
			 query_vals[n_updates].nul = 0;
			 query_vals[n_updates].val.str_val = tuple->location.floor;
			 n_updates++;
		    }
		    if (tuple->location.room.len && tuple->location.room.s) {
			 query_cols[n_updates] = "room";
			 query_vals[n_updates].type = DB_STR;
			 query_vals[n_updates].nul = 0;
			 query_vals[n_updates].val.str_val = tuple->location.room;
			 n_updates++;
		    }
	       }
	       if (tuple->location.x != 0) {
		    query_cols[n_updates] = "x";
		    query_vals[n_updates].type = DB_DOUBLE;
		    query_vals[n_updates].nul = 0;
		    query_vals[n_updates].val.double_val = tuple->location.x;
		    n_updates++;
	       }
	       if (tuple->location.y != 0) {
		    query_cols[n_updates] = "y";
		    query_vals[n_updates].type = DB_DOUBLE;
		    query_vals[n_updates].nul = 0;
		    query_vals[n_updates].val.double_val = tuple->location.y;
		    n_updates++;
	       }
	       if (tuple->location.radius != 0) {
		    query_cols[n_updates] = "radius";
		    query_vals[n_updates].type = DB_DOUBLE;
		    query_vals[n_updates].nul = 0;
		    query_vals[n_updates].val.double_val = tuple->location.radius;
		    n_updates++;
	       }
	       if (tuple->priority != 0.0) {
		    query_cols[n_updates] = "priority";
		    query_vals[n_updates].type = DB_DOUBLE;
		    query_vals[n_updates].nul = 0;
		    query_vals[n_updates].val.double_val = tuple->priority;
		    n_updates++;
	       }
	       if (tuple->expires != 0) {
		    query_cols[n_updates] = "expires";
		    query_vals[n_updates].type = DB_DATETIME;
		    query_vals[n_updates].nul = 0;
		    query_vals[n_updates].val.time_val = tuple->expires;
		    n_updates++;
	       }

	       if (n_updates > (sizeof(query_cols)/sizeof(db_key_t)))
		    LOG(L_ERR, "too many update values. n_selectors=%d, n_updates=%d "
					"dbf.update=%p\n", 
			n_selectors, n_updates, pa_dbf.update);

	       if (pa_dbf.use_table(pa_db, presentity_contact_table) < 0) {
		       LOG(L_ERR, "db_update_presentity: Error in use_table\n");
		       return -1;
	       }

	       if (pa_dbf.insert(pa_db, query_cols, query_vals, n_updates) < 0) {
		    LOG(L_ERR, "db_update_presentity: Error while updating database\n");
		    return -1;
	       }
	  }
     }
     return 0;
}


/*
 * Create a new presence_tuple
 */
int new_presence_tuple(str* _contact, time_t expires, presentity_t *_p, presence_tuple_t ** _t)
{
     presence_tuple_t* tuple;
     int size = 0;

     if (!_contact || !_t) {
	  paerrno = PA_INTERNAL_ERROR;
	  LOG(L_ERR, "new_presence_tuple(): Invalid parameter value\n");
	  return -1;
     }

     size = sizeof(presence_tuple_t) + TUPLE_STATUS_STR_LEN + TUPLE_LOCATION_STR_LEN + _contact->len + 1;
     tuple = (presence_tuple_t*)shm_malloc(size);
     if (!tuple) {
	  paerrno = PA_NO_MEMORY;
	  LOG(L_ERR, "new_presence_tuple(): No memory left: size=%d\n", size);
	  return -1;
     }
     memset(tuple, 0, sizeof(presence_tuple_t));


     tuple->state = PS_UNKNOWN;
     tuple->contact.s = ((char*)tuple) + sizeof(presence_tuple_t) + TUPLE_LOCATION_STR_LEN + TUPLE_STATUS_STR_LEN;
     tuple->status.s = ((char*)tuple) + sizeof(presence_tuple_t) + TUPLE_LOCATION_STR_LEN;
     strncpy(tuple->contact.s, _contact->s, _contact->len);
     _contact->s[_contact->len] = 0;
     tuple->contact.len = _contact->len;
     tuple->location.loc.s = ((char*)tuple) + sizeof(presence_tuple_t) + TUPLE_LOCATION_LOC_OFFSET;
     tuple->location.site.s = ((char*)tuple) + sizeof(presence_tuple_t) + TUPLE_LOCATION_SITE_OFFSET;
     tuple->location.floor.s = ((char*)tuple) + sizeof(presence_tuple_t) + TUPLE_LOCATION_FLOOR_OFFSET;
     tuple->location.room.s = ((char*)tuple) + sizeof(presence_tuple_t) + TUPLE_LOCATION_ROOM_OFFSET;
     tuple->location.packet_loss.s = ((char*)tuple) + sizeof(presence_tuple_t) + TUPLE_LOCATION_PACKET_LOSS_OFFSET;
     tuple->expires = expires;
     tuple->priority = default_priority;

     *_t = tuple;

     LOG(L_ERR, "new_tuple=%p for aor=%.*s contact=%.*s\n", tuple, 
	 _p->uri.len, _p->uri.s,
	 tuple->contact.len, tuple->contact.s);
	
     return 0;
}

/*
 * Find a presence_tuple for contact _contact on presentity _p
 */
int find_presence_tuple(str* _contact, presentity_t *_p, presence_tuple_t ** _t)
{
	presence_tuple_t *tuple;
	if (!_contact || !_contact->len || !_p || !_t) {
		paerrno = PA_INTERNAL_ERROR;
		LOG(L_ERR, "find_presence_tuple(): Invalid parameter value\n");
		return -1;
	}
	tuple = _p->tuples;
	LOG(L_ERR, "find_presence_tuple: _p=%p _p->tuples=%p\n", _p, _p->tuples);
	while (tuple) {
		if (str_strcasecmp(&tuple->contact, _contact) == 0) {
			*_t = tuple;
			return 0;
		}
		tuple = tuple->next;
	}
	return 1;
}

void add_presence_tuple(presentity_t *_p, presence_tuple_t *_t)
{
	presence_tuple_t *tuples = _p->tuples;
	_p->tuples = _t;
	_t->next = tuples;
	if (tuples) {
		tuples->prev = _t;
	}
}

void remove_presence_tuple(presentity_t *_p, presence_tuple_t *_t)
{
	presence_tuple_t *tuples = _p->tuples;
	if (tuples == _t) {
		_p->tuples = _t->next;
	}
	if (_t->prev) {
		_t->prev->next = _t->next;
	}
	if (_t->next) {
		_t->next->prev = _t->prev;
	}
}


/*
 * Free all memory associated with a presence_tuple
 */
void free_presence_tuple(presence_tuple_t * _t)
{
	shm_free(_t);
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
	watcher_t* watcher, *t;
	presence_tuple_t *tuple;

	if (_p && _p->flags)
	     LOG(L_ERR, "timer_presentity: _p=%p %s flags=%x watchers=%p\n", _p, _p->uri.s, _p->flags, _p->watchers);
	if (_p->flags & PFLAG_WATCHERINFO_CHANGED) {
		watcher_t *w = _p->watchers;
		while (w) {
		     if (w && w->flags)
			  LOG(L_ERR, "\t w=%p %s flags=%x\n", w, w->uri.s, w->flags);
		     if (w->flags & WFLAG_SUBSCRIPTION_CHANGED) {
				if (send_notify(_p, w) < 0) {
					LOG(L_ERR, "handle_subscription(): Error while sending notify\n");
					/* FIXME: watcher and presentity should be test for removal here
					 * (and possibly in other error cases too
					 */
				}
				w->flags &= ~WFLAG_SUBSCRIPTION_CHANGED;
			}
			w = w->next;
		}

		notify_winfo_watchers(_p);
		     /* We remove it here because a notify needs to be send first */
		// if (w->expires == 0) free_watcher(w);
		// if (p->slot == 0) free_presentity(p);
	}

	if (_p->flags & (PFLAG_PRESENCE_CHANGED
			|PFLAG_PRESENCE_LISTS_CHANGED
			|PFLAG_XCAP_CHANGED
			|PFLAG_LOCATION_CHANGED)) {
		notify_watchers(_p);
	}

	tuple = _p->tuples;
	while (tuple) {
	  presence_tuple_t *next_tuple = tuple->next;
	  if (tuple->expires < act_time) {
	    LOG(L_ERR, "Expiring tuple %.*s\n", tuple->contact.len, tuple->contact.s);
	    remove_presence_tuple(_p, tuple);
	  }
	  tuple = next_tuple;
	}

	watcher = _p->watchers;

	if (0) print_presentity(stdout, _p);
	while(watcher) {
	        if (watcher->expires <= act_time) {
		  LOG(L_ERR, "Removing watcher %.*s\n", watcher->uri.len, watcher->uri.s);
			watcher->expires = 0;
			send_notify(_p, watcher);
			t = watcher;
			watcher = watcher->next;
			remove_watcher(_p, t);
			free_watcher(t);
			continue;
		}
		
		watcher = watcher->next;
	}

	watcher = _p->winfo_watchers;

	while(watcher) {
	        if (watcher->expires <= act_time) {
		  LOG(L_ERR, "Removing watcher %.*s\n", watcher->uri.len, watcher->uri.s);
			watcher->expires = 0;
			send_notify(_p, watcher);
			t = watcher;
			watcher = watcher->next;
			remove_winfo_watcher(_p, t);
			free_watcher(t);
			continue;
		}
		
		watcher = watcher->next;
	}
	return 0;
}


/*
 * Add a new watcher to the list
 */
int add_watcher(presentity_t* _p, str* _uri, time_t _e, int event_package, doctype_t _a, dlg_t* _dlg, 
		str *_dn, struct watcher** _w)
{
	if (new_watcher(_p, _uri, _e, event_package, _a, _dlg, _dn, _w) < 0) {
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
	watcher_t* watcher, *prev;

	watcher = _p->watchers;
	prev = 0;
	
	while(watcher) {
		if (watcher == _w) {
			if (prev) {
				prev->next = watcher->next;
			} else {
				_p->watchers = watcher->next;
			}
			return 0;
		}

		prev = watcher;
		watcher = watcher->next;
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
	struct watcher* watcher = _p->watchers;

	while(watcher) {
		send_notify(_p, watcher);
		watcher = watcher->next;
	}
	/* clear the flags */
	_p->flags &= ~(PFLAG_PRESENCE_CHANGED
		       |PFLAG_PRESENCE_LISTS_CHANGED
		       |PFLAG_XCAP_CHANGED
		       |PFLAG_LOCATION_CHANGED);
	_p->flags &= ~(PFLAG_PRESENCE_LISTS_CHANGED | PFLAG_WATCHERINFO_CHANGED);
	return 0;
}

/*
 * Notify all winfo watchers in the list
 */
int notify_winfo_watchers(presentity_t* _p)
{
	struct watcher* watcher;

	watcher = _p->winfo_watchers;

	if (watcher)
	  LOG(L_ERR, "notify_winfo_watchers: presentity=%.*s winfo_watchers=%p\n", _p->uri.len, _p->uri.s, watcher);
	while(watcher) {
		LOG(L_ERR, "notify_winfo_watchers: watcher=%.*s\n", watcher->uri.len, watcher->uri.s);
		send_notify(_p, watcher);
		watcher = watcher->next;
	}
	/* clear the watcherinfo changed flag */
	_p->flags &= ~PFLAG_WATCHERINFO_CHANGED;
	return 0;
}


/*
 * Add a new watcher to the winfo_watcher list
 */
int add_winfo_watcher(presentity_t* _p, str* _uri, time_t _e, int event_package, doctype_t _a, dlg_t* _dlg, 
		      str *_dn, struct watcher** _w)
{
	if (new_watcher(_p, _uri, _e, event_package, _a, _dlg, _dn, _w) < 0) {
		LOG(L_ERR, "add_winfo_watcher(): Error while creating new watcher structure\n");
		return -1;
	}

	(*_w)->accept = DOC_WINFO;
	(*_w)->next = _p->winfo_watchers;
	_p->winfo_watchers = *_w;
	return 0;
}


/*
 * Remove a watcher from the list
 */
int remove_winfo_watcher(presentity_t* _p, watcher_t* _w)
{
	watcher_t* watcher, *prev;

	watcher = _p->winfo_watchers;
	prev = 0;
	
	while(watcher) {
		if (watcher == _w) {
			if (prev) {
				prev->next = watcher->next;
			} else {
				_p->winfo_watchers = watcher->next;
			}
			return 0;
		}

		prev = watcher;
		watcher = watcher->next;
	}
	
	     /* Not found */
	DBG("remove_winfo_watcher(): Watcher not found in the list\n");
	return 1;
}

/*
 * Find a given watcher in the list
 */
int find_watcher(struct presentity* _p, str* _uri, int _et, watcher_t** _w)
{
	watcher_t* watcher;

	/* first look for watchers */
	watcher = _p->watchers;

	if (_et != EVENT_PRESENCE_WINFO) {
		while(watcher) {
			if ((_uri->len == watcher->uri.len) &&
			    (!memcmp(_uri->s, watcher->uri.s, _uri->len)) &&
			    (watcher->event_package == _et)) {

				*_w = watcher;
				return 0;
			}
			
			watcher = watcher->next;
		}
	} else {

		/* now look for winfo watchers */
		watcher = _p->winfo_watchers;

		while(watcher) {
			if ((_uri->len == watcher->uri.len) &&
			    (!memcmp(_uri->s, watcher->uri.s, _uri->len)) &&
			    (watcher->event_package == _et)) {

				*_w = watcher;
				return 0;
			}
			
			watcher = watcher->next;
		}
	}
	
	return 1;
}

resource_list_t *resource_list_append_unique(resource_list_t *list, str *uri)
{
	resource_list_t *head = list;
	resource_list_t *last = NULL;
	fprintf(stderr, "resource_lists_append_unique: list=%p uri=%.*s\n", list, uri->len, uri->s);
	while (list) {
		if (str_strcasecmp(&list->uri, uri) == 0)
		     return head;
		last = list;
		list = list->next;
	}
	list = (resource_list_t *)shm_malloc(sizeof(resource_list_t) + uri->len + 1);
	list->uri.len = uri->len;
	list->uri.s = ((char*)list) + sizeof(resource_list_t);
	strncpy(list->uri.s, uri->s, uri->len);
	list->uri.s[uri->len] = 0;
	if (last) {
		list->prev = last;
		last->next = list;
	}
	if (head) {
		return head;
	} else {
		return list;
	}
}

resource_list_t *resource_list_remove(resource_list_t *list, str *uri)
{
	resource_list_t *head = list;
	resource_list_t *last = NULL;
	resource_list_t *next = NULL;
	while (list) {
		if (str_strcasecmp(&list->uri, uri) == 0)
			goto remove;
		last = list;
		list = list->next;
	}
	return head;
 remove:
	next = list->next;
	if (last)
		last->next = next;
	if (next)
		next->prev = last;

	shm_free(list);

	if (head == list)
		return next;
	else
		return head;
}

/*
 * Create a new presentity but no watcher list
 */
int create_presentity_only(struct sip_msg* _m, struct pdomain* _d, str* _puri, 
			   struct presentity** _p)
{
	event_t *parsed_event;
	int et = EVENT_PRESENCE;

	if (_m && _m->event) {
		parsed_event = (event_t *)_m->event->parsed;
		et = parsed_event->parsed;
	}

	if (new_presentity(_d, _puri, _p) < 0) {
		LOG(L_ERR, "create_presentity_only(): Error while creating presentity\n");
		return -2;
	}

	add_presentity(_d, *_p);

	return 0;
}


int pdomain_load_presentities(pdomain_t *pdomain)
{
     if (use_db) {
	  db_key_t query_cols[1];
	  db_op_t  query_ops[1];
	  db_val_t query_vals[1];

	  db_key_t result_cols[4];
	  db_res_t *res;
	  int n_query_cols = 0;
	  int n_result_cols = 0;
	  int uri_col;
	  int presid_col;
	  int i;

	  query_cols[n_query_cols] = "pdomain";
	  query_ops[n_query_cols] = OP_EQ;
	  query_vals[n_query_cols].type = DB_STR;
	  query_vals[n_query_cols].nul = 0;
	  query_vals[n_query_cols].val.str_val = *pdomain->name;
	  n_query_cols++;

	  result_cols[uri_col = n_result_cols++] = "uri";
	  result_cols[presid_col = n_result_cols++] = "presid";

	  if (pa_dbf.use_table(pa_db, presentity_table) < 0) {
		  LOG(L_ERR, "pdomain_load_presentities: Error in use_table\n");
		  return -1;
	  }

	  if (pa_dbf.query (pa_db, query_cols, query_ops, query_vals,
			result_cols, n_query_cols, n_result_cols, 0, &res) < 0) {
	       LOG(L_ERR, "pdomain_load_presentities: Error while querying presentity\n");
	       return -1;
	  }
	  if (res) {
	       for (i = 0; i < res->n; i++) {
		    presentity_t *presentity = NULL;
		    /* fill in tuple structure from database query result */
		    db_row_t *row = &res->rows[i];
		    db_val_t *row_vals = ROW_VALUES(row);
		    int presid = row_vals[presid_col].val.int_val;
		    str uri;
		    if (!row_vals[uri_col].nul) {
			 uri.s = row_vals[uri_col].val.string_val;
			 uri.len = strlen(uri.s);
		    }

		    LOG(L_INFO, "pdomain_load_presentities: pdomain=%.*s presentity uri=%.*s presid=%d\n",
			pdomain->name->len, pdomain->name->s, uri.len, uri.s, presid);

		    new_presentity_no_wb(pdomain, &uri, &presentity);
		    if (presentity) {
			 add_presentity(pdomain, presentity);
			 presentity->presid = presid;
		    }
	       }
	       pa_dbf.free_result(pa_db, res);
	  }
	  
	  { 
	       presentity_t *presentity;
	       for (presentity = pdomain->first; presentity; presentity = presentity->next) {
		    db_read_watcherinfo(presentity);
	       }
	  }
     }
     return 0;
}
