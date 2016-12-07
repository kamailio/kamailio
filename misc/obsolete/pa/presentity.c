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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../lib/srdb2/db.h"
#include "../../dprint.h"
#include "../../id.h"
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
#include "qsa_interface.h"
#include <cds/logger.h>
#include <cds/dbid.h>
#include "async_auth.h"
#include "tuple.h"
#include "pres_notes.h"
#include "extension_elements.h"

extern int use_db;

int auth_rules_refresh_time = 300;

/* ----- helper functions ----- */

int get_presentity_uid(str *uid_dst, struct sip_msg *m)
{
	/* Independently on implementation of get_to_uid this function
	 * gets UID from the message. It never uses dynamic allocation of
	 * data (better to use static buffer instead due to speed)! */
	return get_to_uid(uid_dst, m);
}

void free_tuple_change_info_content(tuple_change_info_t *i)
{
	str_free_content(&i->user);
	str_free_content(&i->contact);
}

int pres_uri2uid(str_t *uid_dst, const str_t *uri)
{
	/* FIXME: convert uri to uid - used by internal subscriptions and fifo
	 * commands - throw it out and use UUID only! */

	struct sip_uri puri;
	
	str_clear(uid_dst);
	
/*	if (db_lookup_user(uri, uid_dst) == 0) return 0; */
	
	/* else try the "hack" */
	if (parse_uri(uri->s, uri->len, &puri) == -1) {
		LOG(L_ERR, "get_from_uid: Error while parsing From URI\n");
		return -1;
	}
	
	str_dup(uid_dst, &puri.user);
	strlower(uid_dst);
	return 0;
}

/* ----- presentity functions ----- */

/* Create a new presentity but do not update database.
 * If pres_id not set it generates new one, but only if db_mode set. */
static inline int new_presentity_no_wb(struct pdomain *pdomain, str* _uri, 
		str *uid, 
		xcap_query_params_t *xcap_params,
		str *pres_id,
		presentity_t** _p)
{
	presentity_t* presentity;
	int size = 0;
	dbid_t id;
	int id_len = 0;
	char *xcap_param_buffer;
	
	if ((!_uri) || (!_p) || (!uid)) {
		paerrno = PA_INTERNAL_ERROR;
		ERR("Invalid parameter value\n");
		return -1;
	}

	if (pres_id) size += pres_id->len;
	else {
		if (use_db) { /* do not generate IDs if not using DB */
			generate_dbid(id);
			id_len = dbid_strlen(id);
			size += id_len;
		}
		else id_len = 0;
	}

	if (xcap_params) size += get_inline_xcap_buf_len(xcap_params);
	size += sizeof(presentity_t) + _uri->len + uid->len;
	presentity = (presentity_t*)mem_alloc(size);
	/* TRACE("allocating presentity: %d\n", size); */
	if (!presentity) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "No memory left: size=%d\n", size);
		*_p = NULL;
		return -1;
	}

	/* fill whole structure with zeros */
	memset(presentity, 0, sizeof(presentity_t));

	msg_queue_init(&presentity->mq);

	presentity->data.uri.s = ((char*)presentity) + sizeof(presentity_t);	
	str_cpy(&presentity->data.uri, _uri);
	presentity->uuid.s = presentity->data.uri.s + presentity->data.uri.len;
	str_cpy(&presentity->uuid, uid);
	presentity->pres_id.s = presentity->uuid.s + presentity->uuid.len;
	if (pres_id) str_cpy(&presentity->pres_id, pres_id);
	else {
		if (use_db) dbid_strcpy(&presentity->pres_id, id, id_len);
		else presentity->pres_id.len = 0;
	}
	xcap_param_buffer = after_str_ptr(&presentity->pres_id);
			
	presentity->pdomain = pdomain;

	if (pa_auth_params.type == auth_xcap) { 
		/* store XCAP parameters for async XCAP queries and refreshing
		 * (FIXME: rewrite - use table of a few of existing XCAP parameter
		 * sets instead of always duplicating because it will be mostly 
		 * the same!) */
		if (dup_xcap_params_inline(&presentity->xcap_params, xcap_params, 
					xcap_param_buffer) < 0) {
			ERR("can't duplicate XCAP parameters\n");
			shm_free(presentity);
			*_p = NULL;
			return -1;
		}
	}
	if (ask_auth_rules(presentity) < 0) {
		/* try it from timer again if fails here */
		presentity->auth_rules_refresh_time = act_time;
	}
	else presentity->auth_rules_refresh_time = act_time + auth_rules_refresh_time;
	
	*_p = presentity;

	/* add presentity into domain */
	add_presentity(pdomain, *_p);

	return 0;
}

static inline int db_add_presentity(presentity_t* presentity)
{
	db_key_t query_cols[6];
	db_val_t query_vals[6];
	int n_query_cols = 0;
	int res;
	str str_xcap_params;
	
	query_cols[0] = col_uri;
	query_vals[0].type = DB_STR;
	query_vals[0].nul = 0;
	query_vals[0].val.str_val = presentity->data.uri;
	n_query_cols++;

	query_cols[n_query_cols] = col_pdomain;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *presentity->pdomain->name;
	n_query_cols++;
	
	query_cols[n_query_cols] = col_uid;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->uuid;
	n_query_cols++;
	
	query_cols[n_query_cols] = col_pres_id;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->pres_id;
	n_query_cols++;
	
	/*query_cols[n_query_cols] = "id_cntr";
	query_vals[n_query_cols].type = DB_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = presentity->id_cntr;
	n_query_cols++;*/

	/* store XCAP parameters with presentity */
	if (xcap_params2str(&str_xcap_params, &presentity->xcap_params) != 0) {
		LOG(L_ERR, "Error while serializing xcap params\n");
		return -1;
	}
	query_cols[n_query_cols] = col_xcap_params;
	query_vals[n_query_cols].type = DB_BLOB;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.blob_val = str_xcap_params;
	n_query_cols++;
	
	res = 0;
	
	if (pa_dbf.use_table(pa_db, presentity_table) < 0) {
		ERR("Error in use_table\n");
		res = -1;
	}

	/* insert new record into database */
	if (res == 0) {
		if (pa_dbf.insert(pa_db, query_cols, query_vals, n_query_cols) < 0) {
			ERR("Error while inserting presentity into DB\n");
			res = -1;
		}
	}
	str_free_content(&str_xcap_params);
	return res;
}

/*
 * Create a new presentity
 */
int new_presentity(struct pdomain *pdomain, str* _uri, str *uid, 
		xcap_query_params_t *params, 
		presentity_t** _p)
{
	int res = 0;

	res = new_presentity_no_wb(pdomain, _uri, uid, params, NULL, _p);
	if (res != 0) return res;
	
	if (use_db) {
		if (db_add_presentity(*_p) != 0) { 
			paerrno = PA_INTERNAL_ERROR;
			free_presentity(*_p);
			*_p = NULL;
			return -1;
		}
	}

	return res;
}

/* Removes all data for presentity (tuples, watchers, tuple notes, ...)
 * from database. It is possible due to that pres_id is unique identifier
 * common for all tables */
int db_remove_presentity_data(presentity_t* presentity, const char *table)
{
	db_key_t keys[] = { col_pres_id };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { { DB_STR, 0, { .str_val = presentity->pres_id } } };
	
	if (!use_db) return 0;
	
	if (pa_dbf.use_table(pa_db, table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, keys, ops, k_vals, 1) < 0) {
		LOG(L_ERR, "Error while querying presentity\n");
		return -1;
	}
	
	return 0;
}

static inline int db_remove_presentity(presentity_t* presentity)
{
	int res = 0;
	
	if (!use_db) return 0;
	
	res = db_remove_presentity_data(presentity, presentity_contact_table);
	res = db_remove_presentity_data(presentity, tuple_notes_table) | res;
	res = db_remove_presentity_data(presentity, watcherinfo_table) | res;
	res = db_remove_presentity_data(presentity, presentity_notes_table) | res;
	res = db_remove_presentity_data(presentity, extension_elements_table) | res;
	res = db_remove_presentity_data(presentity, presentity_table) | res;
	
	return res;
}

void release_presentity(presentity_t *_p)
{
	/* remove presentity from DB and free its memory */
	if (_p) {
		db_remove_presentity(_p);
		free_presentity(_p);
	}
}

void free_presentity(presentity_t* _p)
{
	watcher_t *w, *nw;
	presence_tuple_t *tuple, *t;
	internal_pa_subscription_t *iw, *niw;
	pa_presence_note_t *n, *nn;
	pa_extension_element_t *e, *ne;

	/* remove presentity from domain */
	remove_presentity(_p->pdomain, _p);
	
	/* watchers should be released already */
	w = _p->first_watcher;
	while (w) {
		nw = w->next;
		free_watcher(w);
		w = nw;
	}

	w = _p->first_winfo_watcher;
	while (w) {
		nw = w->next;
		free_watcher(w);
		w = nw;
	}
	
	t = get_first_tuple(_p);
	while (t) {
		tuple = t;
		t = (presence_tuple_t*)t->data.next;
		free_presence_tuple(tuple);
	}

	iw = _p->first_qsa_subscription;
	while (iw) {
		niw = iw->next;
		free_internal_subscription(iw);
		iw = niw;
	}
	
	/* remove notes */
	n = (pa_presence_note_t*)_p->data.first_note;
	while (n) {
		nn = (pa_presence_note_t*)n->data.next;
		free_pres_note(n);
		n = nn;
	}

	/* remove extension_elements */
	e = (pa_extension_element_t*)_p->data.first_unknown_element;
	while (e) {
		ne = (pa_extension_element_t*)e->data.next;
		free_pa_extension_element(e);
		e = ne;
	}

	if (_p->authorization_info) {
		free_pres_rules(_p->authorization_info);
	}

	/* XCAP params are allocated "inline" -> no free needed - it will 
	 * be freed with whole structure */
	/* free_xcap_params_content(&_p->xcap_params); */
	msg_queue_destroy(&_p->mq);
	mem_free(_p);
}

int pdomain_load_presentities(pdomain_t *pdomain)
{
	if (!use_db) return 0;

	db_key_t query_cols[1];
	db_op_t  query_ops[1];
	db_val_t query_vals[1];

	db_key_t result_cols[8];
	db_res_t *res;
	int n_query_cols = 0;
	int n_result_cols = 0;
	int uri_col;
	int presid_col;
	int uid_col;
	int xcap_col;
	int i;
	presentity_t *presentity = NULL;
	db_con_t* db = create_pa_db_connection(); /* must create its own connection (called before child init)! */

	if (!db) {
		ERR("Can't load presentities - no DB connection\n");
		return -1;
	}

	act_time = time(NULL); /* needed for fetching auth rules, ... */

	query_cols[n_query_cols] = col_pdomain;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *pdomain->name;
	n_query_cols++;

	result_cols[uri_col = n_result_cols++] = col_uri;
	result_cols[presid_col = n_result_cols++] = col_pres_id;
	result_cols[uid_col = n_result_cols++] = col_uid;
	result_cols[xcap_col = n_result_cols++] = col_xcap_params;

	if (pa_dbf.use_table(db, presentity_table) < 0) {
		LOG(L_ERR, "pdomain_load_presentities: Error in use_table\n");
		close_pa_db_connection(db);
		return -1;
	}

	if (pa_dbf.query (db, query_cols, query_ops, query_vals,
			result_cols, n_query_cols, n_result_cols, 0, &res) < 0) {
		LOG(L_ERR, "pdomain_load_presentities: Error while querying presentity\n");
		close_pa_db_connection(db);
		return -1;
	}
	if (res) {
		for (i = 0; i < res->n; i++) {
			/* fill in tuple structure from database query result */
			db_row_t *row = &res->rows[i];
			db_val_t *row_vals = ROW_VALUES(row);
			str uri = STR_NULL;
			str pres_id = STR_NULL;
			str uid = STR_NULL;
			str serialized_xcap_params = STR_NULL;
			xcap_query_params_t xcap_params;
			
			if (!row_vals[uri_col].nul) {
				uri.s = (char *)row_vals[uri_col].val.string_val;
				uri.len = strlen(uri.s);
			}
			if (!row_vals[uid_col].nul) {
				uid.s = (char *)row_vals[uid_col].val.string_val;
				uid.len = strlen(uid.s);
			}
			if (!row_vals[presid_col].nul) {
				pres_id.s = (char *)row_vals[presid_col].val.string_val;
				pres_id.len = strlen(pres_id.s);
			}
			if (!row_vals[xcap_col].nul) {
				serialized_xcap_params.s = (char *)row_vals[xcap_col].val.string_val;
				serialized_xcap_params.len = strlen(serialized_xcap_params.s);
			}

			DBG("pdomain_load_presentities: pdomain=%.*s presentity uri=%.*s presid=%.*s\n",
					pdomain->name->len, pdomain->name->s, uri.len, uri.s, 
					pres_id.len, pres_id.s);

			str2xcap_params(&xcap_params, &serialized_xcap_params);
			new_presentity_no_wb(pdomain, &uri, &uid, &xcap_params, &pres_id, &presentity);
			free_xcap_params_content(&xcap_params);
		}
		pa_dbf.free_result(db, res);
	}

	for (presentity = pdomain->first; presentity; presentity = presentity->next) {
		db_read_watcherinfo(presentity, db);
		db_read_tuples(presentity, db);
		db_read_notes(presentity, db);
		db_read_extension_elements(presentity, db);
	}
	
	close_pa_db_connection(db);
	return 0;
}

int set_auth_rules(presentity_t *p, presence_rules_t *new_auth_rules)
{
	watcher_t *w;
	watcher_status_t s;
	
	/* ! call from locked region only ! */
/*	INFO("setting auth rules\n"); */
	
	if (p->authorization_info) {
		free_pres_rules(p->authorization_info); /* free old rules */
	}

	p->authorization_info = new_auth_rules;
	
	/* reauthorize all watchers (but NOT winfo watchers - not needed
	 * now because we have only "implicit" auth. rules for them) */
	
	w = p->first_watcher;
	while (w) {
		s = authorize_watcher(p, w);
		if (w->status != s) {
			/* status has changed */
			w->status = s;
			w->flags |= WFLAG_SUBSCRIPTION_CHANGED;
			p->flags |= PFLAG_WATCHERINFO_CHANGED;
			/* NOTIFYs, terminating, ... wil be done in timer */
		}
		w = w->next;
	}
	
	return 0;
}
