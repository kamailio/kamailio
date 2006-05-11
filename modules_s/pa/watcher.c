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

#include "paerrno.h"
#include "../../db/db.h"
#include "../../dprint.h"
#include "../../parser/parse_event.h"
#include "../../mem/shm_mem.h"
#include "../../trim.h"
#include "../../ut.h"
#include "pa_mod.h"
#include "common.h"
#include "watcher.h"
#include "presentity.h"
#include "auth.h"
#include "ptime.h"

str watcher_status_names[] = {
     [WS_PENDING] = STR_STATIC_INIT("pending"),
     [WS_ACTIVE] = STR_STATIC_INIT("active"),
     [WS_REJECTED] = STR_STATIC_INIT("rejected"),
     [WS_TERMINATED] = STR_STATIC_INIT("terminated"),
     [WS_PENDING_TERMINATED] = STR_STATIC_INIT("terminated"),
     STR_NULL
};

str watcher_event_names[] = {
     [WE_SUBSCRIBE]   = STR_STATIC_INIT("subscribe"),
     [WE_APPROVED]    = STR_STATIC_INIT("approved"),
     [WE_DEACTIVATED] = STR_STATIC_INIT("deactivated"),
     [WE_PROBATION]   = STR_STATIC_INIT("probation"),
     [WE_REJECTED]    = STR_STATIC_INIT("rejected"),
     [WE_TIMEOUT]     = STR_STATIC_INIT("timeout"),
     [WE_GIVEUP]      = STR_STATIC_INIT("giveup"),
     [WE_NORESOURCE]  = STR_STATIC_INIT("noresource"),
     STR_NULL
};

const char *event_package2str(int et) /* FIXME: change working with this to enum ?*/
{
	/* added due to incorrect package handling */
	switch (et) {
		case EVENT_PRESENCE: return "presence";
		case EVENT_PRESENCE_WINFO: return "presence.winfo";
		/*case EVENT_XCAP_CHANGE: return ...; */
		default: return "unknown";
	}
}

int str2event_package(const char *epname) 
{
	/* work only with packages supported by PA! */
	if (strcmp(epname, "presence") == 0) return EVENT_PRESENCE;
	if (strcmp(epname, "presence.winfo") == 0) return EVENT_PRESENCE_WINFO;
	return -1; /* unsupported */
}

/* returns 0 if package supported by PA */
int verify_event_package(int et)
{
	switch (et) {
		case EVENT_PRESENCE: return 0;
		case EVENT_PRESENCE_WINFO: 
			if (watcherinfo_notify) return 0;
			else return -1;
		default: return -1;
	}
}

/*int event_package_from_string(str *epname) 
{
     int i;
     for (i = 0; event_package_name[i]; i++) {
	  if (strcasecmp(epname->s, event_package_name[i]) == 0) {
	       return i;
	  }
     }
     return 0;
}*/

watcher_status_t watcher_status_from_string(str *wsname) 
{
	int i;
	for (i = 0; watcher_status_names[i].len; i++) {
		if (str_strcasecmp(wsname, &watcher_status_names[i]) == 0) {
			return i;
		}
	}
	return 0;
}

watcher_event_t watcher_event_from_string(str *wename) 
{
     int i;
     for (i = 0; watcher_event_names[i].len; i++) {
	  if (str_strcasecmp(wename, &watcher_event_names[i]) == 0) {
	       return i;
	  }
     }
     return 0;
}

#define S_ID_LEN 64

/* be sure s!=NULL */
/* compute a hash value for a string */
unsigned int compute_hash(unsigned int _h, char* s, int len)
{
	#define h_inc h+=v^(v>>3);
		
	char* p;
	register unsigned v;
	register unsigned h = _h;

	for(p=s; p<=(s+len-4); p+=4)
	{
		v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
		h_inc;
	}
	
	v=0;
	for(;p<(s+len); p++)
	{
		v<<=8;
		v+=*p;
	}
	h_inc;

	return h;
}

static int watcher_assign_statement_id(presentity_t *presentity, watcher_t *watcher)
{
	watcher->s_id.len = sprintf(watcher->s_id.s, "SID%dx%px%dx%d", 
			presentity->presid, watcher, (int)time(NULL), rand());
	return 0;
}

/*
 * Create a new watcher structure but do not write to database
 */
int new_watcher_no_wb(presentity_t *_p, str* _uri, time_t _e, int event_package, 
		int doc_type, dlg_t* _dlg, str *_dn, str *server_contact, watcher_t** _w)
{
	watcher_t* watcher;
	int size;

	/* Check parameters */
	if (!_uri && !_dlg && !_w) {
		LOG(L_ERR, "new_watcher(): Invalid parameter value\n");
		return -1;
	}

	/* Allocate memory buffer for watcher_t structure and uri string */
	size = sizeof(watcher_t) + _uri->len + _dn->len + S_ID_LEN + server_contact->len;
	watcher = (watcher_t*)mem_alloc(size);
	TRACE("allocating watcher: %d\n", size);
	if (!watcher) {
		paerrno = PA_NO_MEMORY;
	        LOG(L_ERR, "new_watcher(): No memory left (%d bytes)\n", size);
		return -1;
	}
	memset(watcher, 0, sizeof(watcher_t));

	/* Copy uri string */
	watcher->uri.s = (char*)watcher + S_ID_LEN + sizeof(watcher_t);
	watcher->uri.len = _uri->len;
	memcpy(watcher->uri.s, _uri->s, _uri->len);
	
	/* Copy display_name string */
	watcher->display_name.s = (char*)watcher + S_ID_LEN + sizeof(watcher_t) + _uri->len;
	watcher->display_name.len = _dn->len;
	memcpy(watcher->display_name.s, _dn->s, _dn->len);
	
	/* Copy server_contact string */
	watcher->server_contact.s = (char*)watcher + S_ID_LEN + sizeof(watcher_t) + _uri->len + _dn->len;
	watcher->server_contact.len = server_contact->len;
	memcpy(watcher->server_contact.s, server_contact->s, server_contact->len);

	watcher->s_id.s = (char*)watcher + sizeof(watcher_t);
	watcher->s_id.len = 0;

	watcher->document_index = 0;
	watcher->event_package = event_package;
	watcher->expires = _e; /* Expires value */
	watcher->preferred_mimetype = doc_type;  /* Accepted document type */
	watcher->dialog = _dlg; /* Dialog handle */
	watcher->event = WE_SUBSCRIBE;
	watcher->status = WS_PENDING;
	
	*_w = watcher;

	return 0;
}

static int set_watcher_db_data(presentity_t *_p, watcher_t *watcher,
		db_key_t *cols, db_val_t *vals, int *col_cnt,
		str *dialog_str /* destination for dialog string -> must be freed after ! */
		)
{
	int n_cols = 0;
	char *package = (char*)event_package2str(watcher->event_package);
	str dialog; /* serialized dialog */

	str_clear(dialog_str);
	
	if (dlg_func.dlg2str(watcher->dialog, &dialog) != 0) {	
		LOG(L_ERR, "Error while serializing dialog\n");
		return -1;
	}
	
	cols[n_cols] = "r_uri";
	vals[n_cols].type = DB_STR;
	vals[n_cols].nul = 0;
	vals[n_cols].val.str_val.s = _p->uri.s;
	vals[n_cols].val.str_val.len = _p->uri.len;
	n_cols++;

	cols[n_cols] = "w_uri";
	vals[n_cols].type = DB_STR;
	vals[n_cols].nul = 0;
	vals[n_cols].val.str_val.s = watcher->uri.s;
	vals[n_cols].val.str_val.len = watcher->uri.len;
	n_cols++;

	cols[n_cols] = "package";
	vals[n_cols].type = DB_STR;
	vals[n_cols].nul = 0;
	vals[n_cols].val.str_val.s = package;
	vals[n_cols].val.str_val.len = strlen(package);
	n_cols++;

	cols[n_cols] = "s_id";
	vals[n_cols].type = DB_STR;
	vals[n_cols].nul = 0;
	vals[n_cols].val.str_val.s = watcher->s_id.s;
	vals[n_cols].val.str_val.len = watcher->s_id.len;
	n_cols++;

	cols[n_cols] = "status";
	vals[n_cols].type = DB_STR;
	vals[n_cols].nul = 0;
	vals[n_cols].val.str_val = watcher_status_names[watcher->status];
	n_cols++;

	cols[n_cols] = "event";
	vals[n_cols].type = DB_STR;
	vals[n_cols].nul = 0;
	vals[n_cols].val.str_val = watcher_event_names[watcher->event];
	n_cols++;

	cols[n_cols] = "display_name";
	vals[n_cols].type = DB_STR;
	vals[n_cols].nul = 0;
	vals[n_cols].val.str_val.s = watcher->display_name.s;
	vals[n_cols].val.str_val.len = watcher->display_name.len;
	n_cols++;
	
	cols[n_cols] = "accepts";
	vals[n_cols].type = DB_INT;
	vals[n_cols].nul = 0;
	vals[n_cols].val.int_val = watcher->preferred_mimetype;
	n_cols++;

	cols[n_cols] = "expires";
	vals[n_cols].type = DB_INT;
	vals[n_cols].nul = 0;
	vals[n_cols].val.int_val = watcher->expires;
	n_cols++;

	cols[n_cols] = "dialog";
	vals[n_cols].type = DB_BLOB;
	vals[n_cols].nul = 0;
	vals[n_cols].val.blob_val = dialog;
	n_cols++;
	
	cols[n_cols] = "server_contact";
	vals[n_cols].type = DB_STR;
	vals[n_cols].nul = 0;
	vals[n_cols].val.str_val = watcher->server_contact;
	n_cols++;
	
	cols[n_cols] = "presid";
	vals[n_cols].type = DB_INT;
	vals[n_cols].nul = 0;
	vals[n_cols].val.int_val = _p->presid;
	n_cols++;

	cols[n_cols] = "doc_index";
	vals[n_cols].type = DB_INT;
	vals[n_cols].nul = 0;
	vals[n_cols].val.int_val = watcher->document_index;
	n_cols++;
	
	*col_cnt = n_cols;

	if (dialog_str) *dialog_str = dialog;
	
	return 0;
}

int db_add_watcher(presentity_t *_p, watcher_t *watcher)
{
	str_t tmp;
	db_key_t query_cols[20];
	db_val_t query_vals[20];
	
	int n_query_cols = 0;

	if (!use_db) return 0;
	
	if (watcher->s_id.len < 1) /* id not assigned yet */
		watcher_assign_statement_id(_p, watcher);
	
	str_clear(&tmp);

	if (pa_dbf.use_table(pa_db, watcherinfo_table) < 0) {
		LOG(L_ERR, "db_add_watcher: Error in use_table\n");
		return -1;
	}
	
	if (set_watcher_db_data(_p, watcher, 
				query_cols, query_vals, &n_query_cols,
				&tmp) != 0) {
		return -1;
	}

	/* insert new record into database */
	if (pa_dbf.insert(pa_db, query_cols, query_vals, n_query_cols) < 0) {
		LOG(L_ERR, "db_add_watcher: Error while inserting watcher\n");
		str_free_content(&tmp);
		return -1;
	}
	str_free_content(&tmp);
	
	return 0;
}

int db_update_watcher(presentity_t *_p, watcher_t *watcher)
{
	str tmp;
	db_key_t query_cols[20];
	db_val_t query_vals[20];
	int n_query_cols = 0;
	
	db_key_t keys[] = { "s_id" };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { { DB_STR, 0, { .str_val = watcher->s_id } } };

	if (!use_db) return 0;

	str_clear(&tmp);
	
	if (pa_dbf.use_table(pa_db, watcherinfo_table) < 0) {
		LOG(L_ERR, "db_update_watcher: Error in use_table\n");
		return -1;
	}
	
	if (set_watcher_db_data(_p, watcher, 
				query_cols, query_vals, &n_query_cols,
				&tmp) != 0) {
		return -1;
	}

	if (pa_dbf.update(pa_db, keys, ops, k_vals, 
				query_cols, query_vals, 1, n_query_cols) < 0) {
		LOG(L_ERR, "Error while updating watcher in DB\n");
		str_free_content(&tmp);
		return -1;
	}
	str_free_content(&tmp);

	return 0;
}

int db_remove_watcher(struct presentity *_p, watcher_t *w)
{
	db_key_t keys[] = { "s_id" };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { { DB_STR, 0, { .str_val = w->s_id } } };

	if (!use_db) return 0;
	
	if (pa_dbf.use_table(pa_db, watcherinfo_table) < 0) {
		LOG(L_ERR, "db_remove_watcher: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, keys, ops, k_vals, 1) < 0) {
		LOG(L_ERR, "Error while deleting watcher from DB\n");
		return -1;
	}

	return 0;
}

/*
 * Create a new watcher structure
 */
/*int new_watcher(presentity_t *_p, str* _uri, time_t _e, int event_package, doctype_t _a, dlg_t* _dlg, 
		str *_dn, str *server_contact, watcher_t** _w)
{
	int rc = 0;

	/ * Check parameters * /
	if (!_uri && !_dlg && !_w) {
		LOG(L_ERR, "new_watcher(): Invalid parameter value\n");
		return -1;
	}

	rc = new_watcher_no_wb(_p, _uri, _e, event_package, _a, _dlg, _dn, server_contact, _w);
	if (rc < 0) return rc;
	if (use_db) {
		rc = db_add_watcher(_p, *_w);
		if (rc != 0) free_watcher(*_w);
	}

	return rc;
}*/


/*
 * Read watcherinfo table from database for presentity _p
 */
int db_read_watcherinfo(presentity_t *_p, db_con_t* db)
{
	db_key_t query_cols[5];
	db_op_t query_ops[5];
	db_val_t query_vals[5];

	str dialog = STR_NULL;
	dlg_t *dlg = NULL;
	db_key_t result_cols[11];
	db_res_t *res;
	int r = 0;
	int n_query_cols = 1;
	int n_result_cols = 0;
	int w_uri_col, s_id_col, event_package_col, status_col, watcher_event_col, 
		display_name_col, accepts_col, expires_col, dialog_col, server_contact_col,
		doc_index_col;
	
	if (!use_db) return 0;
	
/*	LOG(L_ERR, "db_read_watcherinfo starting\n");*/
	query_cols[0] = "presid";
	query_ops[0] = OP_EQ;
	query_vals[0].type = DB_INT;
	query_vals[0].nul = 0;
	query_vals[0].val.int_val = _p->presid;
	LOG(L_DBG, "db_read_watcherinfo:  _p->uri='%s', presid=%d\n", 
			_p->uri.s, _p->presid);

	result_cols[w_uri_col = n_result_cols++] = "w_uri";
	result_cols[s_id_col = n_result_cols++] = "s_id";
	result_cols[event_package_col = n_result_cols++] = "package";
	result_cols[status_col = n_result_cols++] = "status";
	result_cols[display_name_col = n_result_cols++] = "display_name";
	result_cols[accepts_col = n_result_cols++] = "accepts";
	result_cols[expires_col = n_result_cols++] = "expires";
	result_cols[watcher_event_col = n_result_cols++] = "event";
	result_cols[dialog_col = n_result_cols++] = "dialog";
	result_cols[server_contact_col = n_result_cols++] = "server_contact";
	result_cols[doc_index_col = n_result_cols++] = "doc_index";
		
	if (pa_dbf.use_table(db, watcherinfo_table) < 0) {
		LOG(L_ERR, "db_read_watcherinfo: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.query (db, query_cols, query_ops, query_vals,
			result_cols, n_query_cols, n_result_cols, 0, &res) < 0) {
		LOG(L_ERR, "db_read_watcherinfo(): Error while querying watcherinfo\n");
		return -1;
	}

	if (res && (res->n > 0)) {
		/* fill in tuple structure from database query result */
		int i;
	
		dlg = NULL;
		for (i = 0; i < res->n; i++) {
			db_row_t *row = &res->rows[i];
			db_val_t *row_vals = ROW_VALUES(row);
			str w_uri = STR_NULL;
			str s_id = STR_NULL;
			char *event_package_str = NULL;
			int event_package = EVENT_PRESENCE;
			str watcher_event_str = STR_NULL;
			watcher_event_t watcher_event = WE_SUBSCRIBE;
			int accepts = row_vals[accepts_col].val.int_val;
			int expires = row_vals[expires_col].val.int_val;
			int doc_index = row_vals[doc_index_col].val.int_val;
			str status = STR_NULL;
			str display_name = STR_NULL;
			str server_contact = STR_NULL;
			watcher_t *watcher = NULL;
			
			if (!row_vals[w_uri_col].nul) {
				w_uri.s = (char *)row_vals[w_uri_col].val.string_val;
				w_uri.len = strlen(w_uri.s);
			}
			if (!row_vals[s_id_col].nul) {
				s_id.s = (char *)row_vals[s_id_col].val.string_val;
				s_id.len = strlen(s_id.s);
			}
			if (!row_vals[event_package_col].nul) {
				event_package_str = (char *)row_vals[event_package_col].val.string_val;
				event_package = str2event_package(event_package_str);
			}
			if (!row_vals[status_col].nul) {
				status.s = (char *)row_vals[status_col].val.string_val;
				status.len = strlen(status.s);
			}
			if (!row_vals[watcher_event_col].nul) {
				watcher_event_str.s = (char *)row_vals[watcher_event_col].val.string_val;
				watcher_event_str.len = strlen(watcher_event_str.s);
				watcher_event = watcher_event_from_string(&watcher_event_str);
			}
			if (!row_vals[display_name_col].nul) {
				display_name.s = (char *)row_vals[display_name_col].val.string_val;
				display_name.len = strlen(display_name.s);
			}
			if (!row_vals[dialog_col].nul) {
				dialog = row_vals[dialog_col].val.blob_val;
				dlg = (dlg_t*)mem_alloc(sizeof(*dlg));
				if (!dlg) {
					LOG(L_ERR, "db_read_watcher: Can't allocate dialog\n");
					r = -1;
				}
				else 
					if (dlg_func.str2dlg(&dialog, dlg) != 0) {	
						LOG(L_ERR, "db_read_watcher: Error while deserializing dialog\n");
						r = -1;
					}
			}
			if (!row_vals[server_contact_col].nul) {
				server_contact.s = (char *)row_vals[server_contact_col].val.string_val;
				server_contact.len = strlen(server_contact.s);
			}

			LOG(L_DBG, "db_read_watcherinfo(): creating watcher\n");
			if (new_watcher_no_wb(_p, &w_uri, expires, 
						event_package, accepts, dlg, &display_name, 
						&server_contact, &watcher) == 0) {
				
				watcher_status_t ws = watcher_status_from_string(&status);
				/* if (watcher->status != ws)
					watcher->flags |= WFLAG_SUBSCRIPTION_CHANGED; */
				watcher->status = ws;
				watcher->event = watcher_event;
				watcher->document_index = doc_index;

				if (s_id.s) {
					strncpy(watcher->s_id.s, s_id.s, S_ID_LEN);
					watcher->s_id.len = strlen(s_id.s);
				}
				if (event_package == EVENT_PRESENCE_WINFO)
					r = add_winfo_watcher(_p, watcher);
				else r = add_watcher(_p, watcher);
				if (r < 0) {
					LOG(L_ERR, "db_read_watcher(): Error while adding watcher\n");
					free_watcher(watcher);
				}
			}
			else r = -1;
		}
	}
	pa_dbf.free_result(db, res);
	LOG(L_DBG, "db_read_watcherinfo:  _p->uri='%s' done\n", _p->uri.s);

	return r;
}

/*
 * Release a watcher structure
 */
void free_watcher(watcher_t* _w)
{
	tmb.free_dlg(_w->dialog);
	mem_free(_w);	
}

/*
 * Update a watcher structure
 */
int update_watcher(struct presentity *p, watcher_t* _w, time_t _e, struct sip_msg *m)
{
	watcher_status_t old = _w->status; /* old status of subscription */
	
	_w->expires = _e;
	_w->flags |= WFLAG_SUBSCRIPTION_CHANGED;
	
	/* actualize watcher's status according to time */
	if (_w->expires <= act_time) {
		/* ERR("Updated watcher to expire: %.*s\n", _w->uri.len, _w->uri.s); */
		_w->expires = 0;
		set_watcher_terminated_status(_w);
	}
	
	/*if (_w->status == WS_PENDING) {*/
	if (!is_watcher_terminated(_w)) {
		/* do reauthorization for non-terminated watchers (policy may
		 * change) - in the future should be done elsewhere using 
		 * "subscriptions to XCAP changes" */
		_w->status = authorize_watcher(p,_w, m);
		/* handle rejected watchers here? */
	}
	
	if ((old != _w->status) && (_w->event_package == EVENT_PRESENCE)) {
		/* changed only when presence watcher changes status  */
		/* FIXME: it could be changed when expires time changes too, 
		 * but we don't send expiration in watcherinf notify thus
		 * it is worthless */
		p->flags |= PFLAG_WATCHERINFO_CHANGED;
	}
	
	if (use_db) return db_update_watcher(p, _w);
	else return 0;
}

int is_watcher_terminated(watcher_t *w)
{
	if (!w) return -1;
	
	if ((w->status == WS_TERMINATED) || 
		(w->status == WS_REJECTED) ||
			(w->status == WS_PENDING_TERMINATED)) return 1;
	return 0;
}

int is_watcher_authorized(watcher_t *w)
{
	if (!w) return 0;
	
	switch (w->status) {
		case WS_PENDING: ;
		case WS_PENDING_TERMINATED: ;
		case WS_REJECTED: return 0;
		case WS_ACTIVE: ;
		case WS_TERMINATED: return 1;
	}
	return 0;
}

void set_watcher_terminated_status(watcher_t *w)
{
	if (!w) return;
	
	switch (w->status) {
		case WS_REJECTED: break;
		case WS_PENDING: ;
			w->status = WS_PENDING_TERMINATED; 
			break;
		case WS_PENDING_TERMINATED: break;
		case WS_TERMINATED: break;
		case WS_ACTIVE: ;
			w->status = WS_TERMINATED; 
			break;
	}
}
