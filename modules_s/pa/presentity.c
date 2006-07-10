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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../db/db.h"
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
#include "location.h"
#include "qsa_interface.h"
#include <cds/logger.h>
#include "async_auth.h"

extern int use_db;
extern char *presentity_table;

int auth_rules_refresh_time = 300;

str pstate_name[PS_NSTATES] = {
	STR_STATIC_INIT("unknown"),
	STR_STATIC_INIT("online"),
	STR_STATIC_INIT("offline"),
	STR_STATIC_INIT("away"),
	STR_STATIC_INIT("xaway"),
	STR_STATIC_INIT("dnd"),
	STR_STATIC_INIT("typing"),
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

int get_presentity_uid(str *uid_dst, struct sip_msg *m)
{
	str s = STR_NULL;
	
	if (!uid_dst) return -1;
	
	if (get_to_uid(&s, m) < 0) {
		str_clear(uid_dst);
		ERR("get_to_uid failed\n");
		return -1;
	}
	
	return str_dup(uid_dst, &s);
}

/*
 * Create a new presentity but do not update database
 */
int new_presentity_no_wb(struct pdomain *pdomain, str* _uri, 
		str *uid, 
		xcap_query_params_t *xcap_params,
		presentity_t** _p)
{
	presentity_t* presentity;
	int size = 0;

	if ((!_uri) || (!_p) || (!uid)) {
		paerrno = PA_INTERNAL_ERROR;
		ERR("Invalid parameter value\n");
		return -1;
	}

	size = sizeof(presentity_t) + _uri->len + 1 + uid->len + 1;
	presentity = (presentity_t*)mem_alloc(size);
	/* TRACE("allocating presentity: %d\n", size); */
	if (!presentity) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "No memory left: size=%d\n", size);
		return -1;
	}
	memset(presentity, 0, sizeof(presentity_t));

	msg_queue_init(&presentity->mq);

	presentity->uri.s = ((char*)presentity) + sizeof(presentity_t);
	strncpy(presentity->uri.s, _uri->s, _uri->len);
	presentity->uri.s[_uri->len] = 0;
	presentity->uri.len = _uri->len;
	presentity->pdomain = pdomain;

	/* these are initialized by memset */
	/* presentity->first_qsa_subscription = 0;
	presentity->last_qsa_subscription = 0;
	presentity->presid = 0;
	presentity->authorization_info = NULL; 
	presentiy->ref_cnt */
	
	presentity->uuid.s = presentity->uri.s + presentity->uri.len + 1;
	strncpy(presentity->uuid.s, uid->s, uid->len);
	presentity->uuid.s[uid->len] = 0;
	presentity->uuid.len = uid->len;

	/* async XCAP query */
	if (dup_xcap_params(&presentity->xcap_params, xcap_params) < 0) {
		ERR("can't duplicate XCAP parameters\n");
		shm_free(presentity);
		*_p = NULL;
		return -1;
	}
	if (ask_auth_rules(presentity) < 0) {
/*		shm_free(presentity);
		free_xcap_params_content(&presentity->xcap_params);
		*_p = NULL;
		return -1; */
		/* try it from timer if fails here */
		presentity->auth_rules_refresh_time = act_time;
	}
	else presentity->auth_rules_refresh_time = act_time + auth_rules_refresh_time;
	
	*_p = presentity;

	/* add presentity into domain */
	add_presentity(pdomain, *_p);

	return 0;
}

static int db_add_presentity(presentity_t* presentity)
{
	db_key_t query_cols[4];
	db_op_t  query_ops[4];
	db_val_t query_vals[4];

	db_key_t result_cols[4];
	db_res_t *res;
	int n_query_cols = 0;
	int n_result_cols = 0;
	int presid_col;
	int presid = 0;
	
	if (!use_db) return 0;
	 
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
	
	query_cols[n_query_cols] = "uid";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->uuid;
	n_query_cols++;

	result_cols[presid_col = n_result_cols++] = "presid";

	if (pa_dbf.use_table(pa_db, presentity_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	while (!presid) {
		if (pa_dbf.query (pa_db, query_cols, query_ops, query_vals,
					result_cols, n_query_cols, n_result_cols, 0, &res) < 0) {
			ERR("Error while querying presentity\n");
			return -1;
		}
		if (res && res->n > 0) {
			/* fill in tuple structure from database query result */
			db_row_t *row = &res->rows[0];
			db_val_t *row_vals = ROW_VALUES(row);
			presid = presentity->presid = row_vals[presid_col].val.int_val;

			DBG("  presid=%d\n", presid);

		} else {
			/* insert new record into database */
			DBG("inserting %d cols into table\n", n_query_cols);
			if (pa_dbf.insert(pa_db, query_cols, query_vals, n_query_cols)
					< 0) {
				ERR("Error while inserting tuple\n");
				return -1;
			}
		}
		pa_dbf.free_result(pa_db, res);
	}
	return 0;
}

static int db_remove_tuples(presentity_t* presentity)
{
	db_key_t query_cols[4];
	db_op_t  query_ops[4];
	db_val_t query_vals[4];

	int n_query_cols = 0;
	
	if (!use_db) return 0;
	 
	query_cols[0] = "presid";
	query_ops[0] = OP_EQ;
	query_vals[0].type = DB_INT;
	query_vals[0].nul = 0;
	query_vals[0].val.int_val = presentity->presid;
	n_query_cols++;

	if (pa_dbf.use_table(pa_db, presentity_contact_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, query_cols, query_ops, query_vals, n_query_cols) < 0) {
		ERR("Error while querying presentity\n");
		return -1;
	}
	
	return 0;
}

static int db_remove_watchers(presentity_t* presentity)
{
	db_key_t query_cols[4];
	db_op_t  query_ops[4];
	db_val_t query_vals[4];

	int n_query_cols = 0;
	
	if (!use_db) return 0;
	 
	query_cols[0] = "presid";
	query_ops[0] = OP_EQ;
	query_vals[0].type = DB_INT;
	query_vals[0].nul = 0;
	query_vals[0].val.int_val = presentity->presid;
	n_query_cols++;

	if (pa_dbf.use_table(pa_db, watcherinfo_table) < 0) {
		LOG(L_ERR, "Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, query_cols, query_ops, query_vals, n_query_cols) < 0) {
		LOG(L_ERR, "Error while querying presentity\n");
		return -1;
	}
	
	return 0;
}

int db_remove_presentity(presentity_t* presentity)
{
	db_key_t query_cols[4];
	db_op_t  query_ops[4];
	db_val_t query_vals[4];
	
	int n_query_cols = 0;
	int res = 0;
	
	if (!use_db) return 0;

	res = db_remove_tuples(presentity);
	res = db_remove_all_tuple_notes(presentity) | res;
	res = db_remove_watchers(presentity) | res;
	res = db_remove_pres_notes(presentity) | res;
	res = db_remove_person_elements(presentity) | res;
	 
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

	if (pa_dbf.use_table(pa_db, presentity_table) < 0) {
		LOG(L_ERR, "Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, query_cols, query_ops, query_vals, n_query_cols) < 0) {
		LOG(L_ERR, "Error while querying presentity\n");
		return -1;
	}
	
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

	res = new_presentity_no_wb(pdomain, _uri, uid, params, _p);
	if (res != 0) {
		return res;
	}
	
	if (use_db) {
		if (db_add_presentity(*_p) != 0) { 
			paerrno = PA_INTERNAL_ERROR;
			free_presentity(*_p);
			return -1;
		}
	}

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

/*
 * Free all memory associated with a presentity
 */
void free_presentity(presentity_t* _p)
{
	watcher_t* ptr;
	presence_tuple_t *tuple;
	internal_pa_subscription_t *iw, *niw;
	pa_presence_note_t *n, *nn;

	/* remove presentity from domain */
	remove_presentity(_p->pdomain, _p);
	
	/* watchers should be released already */
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

	iw = _p->first_qsa_subscription;
	while (iw) {
		niw = iw->next;
		free_internal_subscription(iw);
		iw = niw;
	}
	
	/* remove notes */
	n = _p->notes;
	while (n) {
		nn = n->next;
		free_pres_note(n);
		n = nn;
	}

	if (_p->authorization_info) {
		free_pres_rules(_p->authorization_info);
	}

	msg_queue_destroy(&_p->mq);
	mem_free(_p);
}

static int db_remove_presence_tuple(presentity_t *_p, presence_tuple_t *t)
{
	db_key_t keys[] = { "presid", "tupleid" };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[] = { { DB_INT, 0, { .int_val = _p->presid } },
		{ DB_STR, 0, { .str_val = t->id } } };
	
	if (!use_db) return 0;
	if (!t->is_published) return 0; /* store only published tuples */

	db_remove_tuple_notes(_p, t);
	
	if (pa_dbf.use_table(pa_db, presentity_contact_table) < 0) {
		LOG(L_ERR, "db_remove_presence_tuple: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, keys, ops, k_vals, 2) < 0) {
		LOG(L_ERR, "db_remove_presence_tuple: Can't delete record\n");
		return -1;
	}
	
	return 0;
}

static int set_tuple_db_data(presentity_t *_p, presence_tuple_t *tuple,
		db_key_t *cols, db_val_t *vals, int *col_cnt)
{
	int n_updates = 0;

	cols[n_updates] = "tupleid";
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = tuple->id;
	n_updates++;
	
	cols[n_updates] = "presid";
	vals[n_updates].type = DB_INT;
	vals[n_updates].nul = 0;
	vals[n_updates].val.int_val = _p->presid;
	n_updates++;
	
	cols[n_updates] = "basic";
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = pstate_name[tuple->state];
	n_updates++;

	cols[n_updates] = "contact";
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = tuple->contact;
	n_updates++;	
	
	cols[n_updates] = "etag";
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = tuple->etag;
	n_updates++;	

	cols[n_updates] = "published_id";
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = tuple->published_id;
	n_updates++;	

	if (tuple->priority != 0.0) {
		cols[n_updates] = "priority";
		vals[n_updates].type = DB_DOUBLE;
		vals[n_updates].nul = 0;
		vals[n_updates].val.double_val = tuple->priority;
		n_updates++;
	}
	if (tuple->expires != 0) {
		cols[n_updates] = "expires";
		vals[n_updates].type = DB_DATETIME;
		vals[n_updates].nul = 0;
		vals[n_updates].val.time_val = tuple->expires;
		n_updates++;
	}
	if (tuple->prescaps != 0) {
		cols[n_updates] = "prescaps";
		vals[n_updates].type = DB_INT;
		vals[n_updates].nul = 0;
		vals[n_updates].val.int_val = tuple->prescaps;
		n_updates++;
	}
	*col_cnt = n_updates;
	return 0;
}

int db_update_presence_tuple(presentity_t *_p, presence_tuple_t *t, int update_notes)
{
	db_key_t keys[] = { "presid", "tupleid" };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[] = { { DB_INT, 0, { .int_val = _p->presid } },
		{ DB_STR, 0, { .str_val = t->id } } };
	
	db_key_t query_cols[20];
	db_val_t query_vals[20];
	int n_query_cols = 0;

	if (!use_db) return 0;
	if (!t->is_published) return 0; /* store only published tuples */

	if (set_tuple_db_data(_p, t, query_cols, 
				query_vals, &n_query_cols) != 0) {
		return -1;
	}
	
	if (pa_dbf.use_table(pa_db, presentity_contact_table) < 0) {
		LOG(L_ERR, "db_update_presence_tuple: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.update(pa_db, keys, ops, k_vals, 
				query_cols, query_vals, 2, n_query_cols) < 0) {
		LOG(L_ERR, "db_update_presence_tuple: Can't update record\n");
		return -1;
	}

	if (update_notes) db_update_tuple_notes(_p, t);
	
	return 0;
}

static int db_add_presence_tuple(presentity_t *_p, presence_tuple_t *t)
{
	db_key_t query_cols[20];
	db_val_t query_vals[20];
	int n_query_cols = 0;

	if (!use_db) return 0;
	if (!t->is_published) return 0; /* store only published tuples */
	
	if (set_tuple_db_data(_p, t, query_cols, 
				query_vals, &n_query_cols) != 0) {
		return -1;
	}
	
	if (pa_dbf.use_table(pa_db, presentity_contact_table) < 0) {
		LOG(L_ERR, "db_add_presence_tuple: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.insert(pa_db, query_cols, query_vals, n_query_cols) < 0) {
		LOG(L_ERR, "db_add_presence_tuple: Can't insert record\n");
		return -1;
	}
		
	return db_add_tuple_notes(_p, t);
}

void set_tuple_published(presentity_t *p, presence_tuple_t *t)
{
	if (!t->is_published) {
		t->is_published = 1;
		db_add_presence_tuple(p, t); /* FIXME: move */
	}
}

void add_presence_tuple_no_wb(presentity_t *_p, presence_tuple_t *_t);

static int db_read_tuples(presentity_t *_p, db_con_t* db)
{
	db_key_t keys[] = { "presid" };
	db_op_t ops[] = { OP_EQ };
	/* db_val_t k_vals[] = { { DB_STR, 0, { .str_val = _p->presid } } }; */
	db_val_t k_vals[] = { { DB_INT, 0, { .int_val = _p->presid } } };

	int i;
	int r = 0;
	db_res_t *res = NULL;
	db_key_t result_cols[] = { "contactid", "basic", "status", 
		"location", "expires", "placeid", 
		"priority", "contact", "tupleid",
		"prescaps", "etag", "published_id"
	} ;
	
	if (!use_db) return 0;

	if (pa_dbf.use_table(db, presentity_contact_table) < 0) {
		LOG(L_ERR, "db_read_tuples: Error in use_table\n");
		return -1;
	}
	
	if (pa_dbf.query (db, keys, ops, k_vals,
			result_cols, 1, sizeof(result_cols) / sizeof(db_key_t), 
			0, &res) < 0) {
		LOG(L_ERR, "db_read_tuples(): Error while querying watcherinfo\n");
		return -1;
	}

	if (!res) return 0; /* ? */
	
	for (i = 0; i < res->n; i++) {
		presence_tuple_t *tuple = NULL;
		db_row_t *row = &res->rows[i];
		db_val_t *row_vals = ROW_VALUES(row);
		str contact = STR_NULL;
		str basic = STR_NULL; 
		str status = STR_NULL; 
		str location = STR_NULL; 
		str id = STR_NULL; 
		str etag = STR_NULL;
		str published_id = STR_NULL;
		
		/* int contactid = row_vals[0].val.int_val; */
		time_t expires = 0;
		/* int placeid = row_vals[5].val.int_val; */
		double priority = row_vals[6].val.double_val;
		int prescaps = row_vals[9].val.int_val;
		
#define get_str_val(i,dst)	do{if(!row_vals[i].nul){dst.s=(char*)row_vals[i].val.string_val;dst.len=strlen(dst.s);}}while(0)
#define get_time_val(i,dst)	do{if(!row_vals[i].nul){dst=row_vals[i].val.time_val;}}while(0)

		get_str_val(1, basic);
		get_str_val(2, status);
		get_str_val(3, location);
		get_time_val(4, expires);
		get_str_val(7, contact);
		get_str_val(8, id);
		get_str_val(10, etag);
		get_str_val(11, published_id);
		
#undef get_str_val		
#undef get_time_val		

		r = new_presence_tuple(&contact, expires, &tuple, 1) | r;
		if (tuple) {
			tuple->state = basic2status(basic);
			memcpy(tuple->status.s, status.s, status.len);
			tuple->status.len = status.len;
			LOG(L_DBG, "read tuple %.*s\n", id.len, id.s);
			memcpy(tuple->id.s, id.s, id.len);
			tuple->id.len = id.len;
			tuple->priority = priority;
			tuple->prescaps = prescaps;
			str_dup(&tuple->etag, &etag);
			str_dup(&tuple->published_id, &published_id);

			db_read_tuple_notes(_p, tuple, db);
			
			add_presence_tuple_no_wb(_p, tuple);
		}
	}
	
	pa_dbf.free_result(db, res);

	return r;
}

/*
 * Sync presentity to db if db is in use
 */
int db_update_presentity(presentity_t* _p)
{
	if (use_db) {
		presence_tuple_t *tuple;
		for (tuple = _p->tuples; tuple; tuple = tuple->next) {
			db_update_presence_tuple(_p, tuple, 0);
		}
	}
	return 0;
}


/*
 * Create a new presence_tuple
 */
int new_presence_tuple(str* _contact, time_t expires, presence_tuple_t ** _t, int is_published)
{
	presence_tuple_t* tuple;
	int size = 0;

	if (!_contact || !_t) {
		paerrno = PA_INTERNAL_ERROR;
		LOG(L_ERR, "new_presence_tuple(): Invalid parameter value\n");
		return -1;
	}

	size = sizeof(presence_tuple_t) + _contact->len + 1;
	tuple = (presence_tuple_t*)mem_alloc(size);
	if (!tuple) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_presence_tuple(): No memory left: size=%d\n", size);
		return -1;
	}
	memset(tuple, 0, sizeof(presence_tuple_t));


	tuple->state = PS_UNKNOWN;
	tuple->contact.s = ((char*)tuple) + sizeof(presence_tuple_t);
	tuple->status.s = tuple->status_buf;
	strncpy(tuple->contact.s, _contact->s, _contact->len);
	tuple->contact.s[_contact->len] = 0;
	tuple->contact.len = _contact->len;
	tuple->id.s = tuple->id_buf;
	tuple->expires = expires;
	tuple->priority = default_priority;
	tuple->is_published = is_published;
	str_clear(&tuple->etag);
	str_clear(&tuple->published_id);

	tuple->id.len = sprintf(tuple->id.s, "%px%xx%x", 
			tuple, rand(), (unsigned int)time(NULL));

	*_t = tuple;

/*	LOG(L_DBG, "new_tuple=%p for aor=%.*s contact=%.*s id=%.*s\n", tuple, 
			_p->uri.len, _p->uri.s,
			tuple->contact.len, tuple->contact.s,
			tuple->id.len, tuple->id.s);*/

	return 0;
}

/*
 * Find a presence_tuple for contact _contact on presentity _p
 */
int find_registered_presence_tuple(str* _contact, presentity_t *_p, presence_tuple_t ** _t)
{
	presence_tuple_t *tuple;
	if (!_contact || !_contact->len || !_p || !_t) {
		paerrno = PA_INTERNAL_ERROR;
		LOG(L_ERR, "find_presence_tuple(): Invalid parameter value\n");
		return -1;
	}
	tuple = _p->tuples;
	while (tuple) {
		/* only contacts from usrloc should be unique - published
		 * may be more times !!! */
		if (!tuple->is_published) {
			if (str_strcasecmp(&tuple->contact, _contact) == 0) {
				*_t = tuple;
				return 0;
			}
		}
		tuple = tuple->next;
	}
	return 1;
}

/*
 * Find a presence_tuple on presentity _p
 */
int find_presence_tuple_id(str* id, presentity_t *_p, presence_tuple_t ** _t)
{
	presence_tuple_t *tuple;
	if (!id || !id->len || !_p || !_t) {
		paerrno = PA_INTERNAL_ERROR;
		LOG(L_ERR, "find_presence_tuple_id(): Invalid parameter value\n");
		return -1;
	}
	tuple = _p->tuples;
	while (tuple) {
		if (str_strcasecmp(&tuple->id, id) == 0) {
			*_t = tuple;
			return 0;
		}
		tuple = tuple->next;
	}
	return 1;
}

void add_presence_tuple_no_wb(presentity_t *_p, presence_tuple_t *_t)
{
	presence_tuple_t *tuples = _p->tuples;
	_p->tuples = _t;
	_t->next = tuples;
	if (tuples) {
		tuples->prev = _t;
	}
}

void add_presence_tuple(presentity_t *_p, presence_tuple_t *_t)
{
	add_presence_tuple_no_wb(_p, _t);
	if (use_db) db_add_presence_tuple(_p, _t); 
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
	
	if (use_db) db_remove_presence_tuple(_p, _t);
}

/*
 * Free all memory associated with a presence_tuple
 */
void free_presence_tuple(presence_tuple_t * _t)
{
	if (_t) {
		str_free_content(&_t->etag);
		str_free_content(&_t->published_id);

		mem_free(_t);
	}
}

static void process_watchers(presentity_t* _p, int *changed)
{
	watcher_t *next, *w, *prev;
	int presentity_changed;
	int notify;
	
	/* !!! "changed" is not initialized here it is only set if change
	 * in presentity occurs */
	
	presentity_changed = _p->flags & PFLAG_PRESENCE_CHANGED;

	prev = NULL;
	w = _p->watchers;
	while (w) {
		/* changes status of expired watcher */
		if (w->expires <= act_time) {
			LOG(L_DBG, "Expired watcher %.*s\n", w->uri.len, w->uri.s);
			w->expires = 0;
			set_watcher_terminated_status(w);
			_p->flags |= PFLAG_WATCHERINFO_CHANGED;
			w->flags |= WFLAG_SUBSCRIPTION_CHANGED;
			if (changed) *changed = 1;
		}

		/* send NOTIFY if needed */
		notify = 0;
		if ((w->flags & WFLAG_SUBSCRIPTION_CHANGED)) {
			notify = 1;
			if (changed) *changed = 1; /* ??? */
		}
		if (presentity_changed && is_watcher_authorized(w)) notify = 1;
		if (notify) send_notify(_p, w);
		w->flags &= ~WFLAG_SUBSCRIPTION_CHANGED;
		
		if (is_watcher_terminated(w)) {
			next = w->next;
			if (prev) prev->next = next;
			else _p->watchers = next;
			if (use_db) db_remove_watcher(_p, w);
			free_watcher(w);
			w = next;
			_p->flags |= PFLAG_WATCHERINFO_CHANGED; /* terminated status could be set before */
			if (changed) *changed = 1;
		}
		else {
			prev = w;
			w = w->next;
		}
	}
}

void remove_watcher_if_expired(presentity_t* _p, watcher_t *w)
{
	int winfo = 0;
	if (!w) return;
	
	if (w->event_package == EVENT_PRESENCE_WINFO) winfo = 1;
	
	if (is_watcher_terminated(w)) {
		if (use_db) db_remove_watcher(_p, w);
		if (winfo) remove_winfo_watcher(_p, w);
		else remove_watcher(_p, w);
		free_watcher(w);
		if (!winfo) _p->flags |= PFLAG_WATCHERINFO_CHANGED;
	}
}

static void process_winfo_watchers(presentity_t* _p, int *changed)
{
	watcher_t *next, *w, *prev;
	int notify;
	
	/* !!! "changed" is not initialized here it is only set if change
	 * in presentity occurs */
	
	prev = NULL;
	w = _p->winfo_watchers;
	while (w) {
		/* changes status of expired watcher */
		if (w->expires <= act_time) {
			LOG(L_DBG, "Expired watcher %.*s\n", w->uri.len, w->uri.s);
			w->expires = 0;
			set_watcher_terminated_status(w);
			w->flags |= WFLAG_SUBSCRIPTION_CHANGED;
			if (changed) *changed = 1;
		}

		/* send NOTIFY if needed */
		notify = 0;
		if ((w->flags & WFLAG_SUBSCRIPTION_CHANGED)) {
			notify = 1;
			if (changed) *changed = 1; /* ??? */
		}
		if ((_p->flags & PFLAG_WATCHERINFO_CHANGED) && 
			is_watcher_authorized(w)) notify = 1;
		if (notify) send_notify(_p, w);
		w->flags &= ~WFLAG_SUBSCRIPTION_CHANGED;
		
		if (is_watcher_terminated(w)) {
			next = w->next;
			if (prev) prev->next = next;
			else _p->winfo_watchers = next;
			if (use_db) db_remove_watcher(_p, w);
			free_watcher(w);
			w = next;
			if (changed) *changed = 1;
		}
		else {
			prev = w;
			w = w->next;
		}
	}
}

/* static void mark_expired_tuples(presentity_t *_p, int *changed)
{
	presence_tuple_t *t;

	t = _p->tuples;
	while (t) {	
		if (t->expires < act_time) {
			t->state = PS_OFFLINE;
			if (changed) *changed = 1;
			_p->flags |= PFLAG_PRESENCE_CHANGED;
		}
		t = t->next;
	}
}*/

static void remove_expired_tuples(presentity_t *_p, int *changed)
{
	presence_tuple_t *t, *n;

	t = _p->tuples;
	while (t) {
		n = t->next;
		if (t->expires < act_time) {
			LOG(L_DBG, "Expiring tuple %.*s\n", t->contact.len, t->contact.s);
			remove_presence_tuple(_p, t);
			free_presence_tuple(t);
			if (changed) *changed = 1;
			_p->flags |= PFLAG_PRESENCE_CHANGED;
		}
		t = n;
	}
}

static void remove_expired_notes(presentity_t *_p)
{
	pa_presence_note_t *n, *nn;

	n = _p->notes;
	while (n) {
		nn = n->next;
		if (n->expires < act_time) {
			LOG(L_DBG, "Expiring note %.*s\n", FMT_STR(n->note));
			remove_pres_note(_p, n);
			_p->flags |= PFLAG_PRESENCE_CHANGED;
		}
		n = nn;
	}
}

static void remove_expired_person_elements(presentity_t *_p)
{
	pa_person_element_t *n, *nn;

	n = _p->person_elements;
	while (n) {
		nn = n->next;
		if (n->expires < act_time) {
			LOG(L_DBG, "Expiring person element %.*s\n", FMT_STR(n->id));
			remove_person_element(_p, n);
			_p->flags |= PFLAG_PRESENCE_CHANGED;
		}
		n = nn;
	}
}

static void process_tuple_change(presentity_t *p, tuple_change_info_t *info)
{
	presence_tuple_t *tuple = NULL;
	int orig;
			

	DBG("processing tuple change message: %.*s, %.*s, %d\n",
			FMT_STR(info->user), FMT_STR(info->contact), info->state);
	if (info->contact.len > 0) {
		tuple = NULL;
		if (find_registered_presence_tuple(&info->contact, p, &tuple) != 0) {
			new_presence_tuple(&info->contact, act_time + default_expires, &tuple, 0);
			add_presence_tuple(p, tuple);
		}
		if (tuple) {
			if (!tuple->is_published) {	/* not overwrite published information */
				orig = tuple->state;
				tuple->state = info->state;
				if (tuple->state == PS_OFFLINE)
					tuple->expires = act_time + 2 * timer_interval;
				else {
					tuple->expires = INT_MAX; /* act_time + default_expires; */
					/* hack - re-registrations don't call the callback */
				}
				db_update_presentity(p);
				
				if (orig != tuple->state) {
					p->flags |= PFLAG_PRESENCE_CHANGED;
				}
			}
		}
	}
}

static int process_qsa_message(presentity_t *p, client_notify_info_t *info)
{
	TRACE("received QSA notification for presentity %.*s\n", FMT_STR(p->uri));

	/* TODO: handle it as publish for special tuple */
	
	return 0;
}

void free_tuple_change_info_content(tuple_change_info_t *i)
{
	str_free_content(&i->user);
	str_free_content(&i->contact);
}

static void process_presentity_messages(presentity_t *p)
{
	mq_message_t *msg;
	tuple_change_info_t *info;
	client_notify_info_t *qsa_info;

	while ((msg = pop_message(&p->mq)) != NULL) {

		/* FIXME: ugly data type detection */
		if (msg->destroy_function == (destroy_function_f)free_tuple_change_info_content) {
			info = (tuple_change_info_t*)get_message_data(msg);
			if (info) process_tuple_change(p, info);
		}
		else {
			/* QSA message */
			qsa_info = (client_notify_info_t *)get_message_data(msg);
			if (qsa_info) process_qsa_message(p, qsa_info);
		}
			
		free_message(msg);
	}
}

static inline int refresh_auth_rules(presentity_t *p)
{
	/* TODO reload authorization rules if needed */
	if ((p->auth_rules_refresh_time > 0) && 
			(p->auth_rules_refresh_time <= act_time)) {
/*		INFO("refreshing auth rules\n"); */
		ask_auth_rules(p); /* it will run next time if fails now */
		p->auth_rules_refresh_time = act_time + auth_rules_refresh_time;
	}
	return 0;
}

int timer_presentity(presentity_t* _p)
{
	int old_flags;
	int presentity_changed;

	PROF_START(pa_timer_presentity)
	old_flags = _p->flags;

	/* reload authorization rules if needed */
	refresh_auth_rules(_p);
	
	process_presentity_messages(_p);
	
	remove_expired_tuples(_p, NULL);
	
	remove_expired_notes(_p);
	remove_expired_person_elements(_p);
	
	/* notify watchers and remove expired */
	process_watchers(_p, NULL);	
	/* notify winfo watchers and remove expired */
	process_winfo_watchers(_p, NULL); 
	
	/* notify internal watchers */
	presentity_changed = _p->flags & PFLAG_PRESENCE_CHANGED;
	if (presentity_changed) {
		/* DBG("presentity %.*s changed\n", _p->uri.len, _p->uri.s); */
		notify_qsa_watchers(_p);
	}

	/* clear presentity "change" flags */
	_p->flags &= ~(PFLAG_PRESENCE_CHANGED | PFLAG_WATCHERINFO_CHANGED);
	
	/* update DB record if something changed - USELESS */
/*	if (changed) {
		db_update_presentity(_p);
	} 
*/	
	PROF_STOP(pa_timer_presentity)
	return 0;
}


/*
 * Add a new watcher to the list
 */
int add_watcher(presentity_t* _p, struct watcher* _w)
{
	_w->next = _p->watchers;
	_p->watchers = _w;
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
			
	LOG(L_DBG, "removing watcher %p (pres %p)\n", _w, _p);
	
	while(watcher) {
		if (watcher == _w) {
			if (prev) {
				prev->next = watcher->next;
			} else {
				_p->watchers = watcher->next;
			}
			LOG(L_DBG, "removed watcher %p (pres %p)\n", _w, _p);
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
		watcher->flags &= ~WFLAG_SUBSCRIPTION_CHANGED; /* FIXME: move it somewhere else */
		watcher = watcher->next;
	}
	/* clear the flags */
	_p->flags &= ~(PFLAG_PRESENCE_CHANGED | PFLAG_WATCHERINFO_CHANGED);
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
	  LOG(L_DBG, "notify_winfo_watchers: presentity=%.*s winfo_watchers=%p\n", _p->uri.len, _p->uri.s, watcher);
	while(watcher) {
		LOG(L_DBG, "notify_winfo_watchers: watcher=%.*s\n", watcher->uri.len, watcher->uri.s);
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
int add_winfo_watcher(presentity_t* _p, struct watcher* _w)
{
	_w->next = _p->winfo_watchers;
	_p->winfo_watchers = _w;
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

/* returns 0 if equal dialog IDs */
static int cmp_dlg_ids(dlg_id_t *a, dlg_id_t *b)
{
	if (!a) {
		if (!b) return -1;
		else return 0;
	}
	if (!b) return 1;

	if (str_case_equals(&a->call_id, &b->call_id) != 0) return 1;
	if (str_case_equals(&a->rem_tag, &b->rem_tag) != 0) return 1; /* case sensitive ? */
	if (str_case_equals(&a->loc_tag, &b->loc_tag) != 0) return 1; /* case sensitive ? */
	return 0;
}

/*
 * Find a watcher in the list via dialog identifier
 */
int find_watcher_dlg(struct presentity* _p, dlg_id_t *dlg_id, int _et, watcher_t** _w)
{
	watcher_t* watcher;

	/* first look for watchers */

	if (!dlg_id) return -1;

	if (_et != EVENT_PRESENCE_WINFO)
		watcher = _p->watchers;
	else	
		watcher = _p->winfo_watchers;
	
	while(watcher) {
		if (watcher->dialog) {
			if ((cmp_dlg_ids(&watcher->dialog->id, dlg_id) == 0) && 
					(watcher->event_package == _et)) {
				*_w = watcher;
				return 0;
			}
		}
		
		watcher = watcher->next;
	}
	
	return 1;
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
	int i;
	presentity_t *presentity = NULL;
	db_con_t* db = create_pa_db_connection(); /* must create its own connection (called before child init)! */

	if (!db) {
		ERR("Can't load presentities - no DB connection\n");
		return -1;
	}

	act_time = time(NULL); /* needed for fetching auth rules, ... */

	query_cols[n_query_cols] = "pdomain";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *pdomain->name;
	n_query_cols++;

	result_cols[uri_col = n_result_cols++] = "uri";
	result_cols[presid_col = n_result_cols++] = "presid";
	result_cols[uid_col = n_result_cols++] = "uid";

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
			int presid = row_vals[presid_col].val.int_val;
			str uri = STR_NULL;
			str uid = STR_NULL;
			
			if (!row_vals[uri_col].nul) {
				uri.s = (char *)row_vals[uri_col].val.string_val;
				uri.len = strlen(uri.s);
			}
			if (!row_vals[uid_col].nul) {
				uid.s = (char *)row_vals[uid_col].val.string_val;
				uid.len = strlen(uid.s);
			}

			DBG("pdomain_load_presentities: pdomain=%.*s presentity uri=%.*s presid=%d\n",
					pdomain->name->len, pdomain->name->s, uri.len, uri.s, presid);

			/* TODO: loading and storing XCAP parameters */
			new_presentity_no_wb(pdomain, &uri, &uid, NULL, &presentity);
			if (presentity) {
				presentity->presid = presid;
			}
		}
		pa_dbf.free_result(db, res);
	}

	for (presentity = pdomain->first; presentity; presentity = presentity->next) {
		db_read_watcherinfo(presentity, db);
		db_read_tuples(presentity, db);
		db_read_notes(presentity, db);
		db_read_person_elements(presentity, db);
	}
	
	close_pa_db_connection(db);
	return 0;
}

/* static int db_lookup_user(str *uri, str *dst_uid); */

int pres_uri2uid(str_t *uid_dst, const str_t *uri)
{
	/* FIXME: convert uri to uid - used by internal subscriptions and fifo commands */

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
	
	w = p->watchers;
	while (w) {
		s = authorize_watcher(p, w);
		if (w->status != s) {
			/* status has changed */
			w->status = s;
			w->flags |= WFLAG_SUBSCRIPTION_CHANGED;
			/* NOTIFYs, terminating, ... wil be done in timer */
		}
		w = w->next;
	}
	
	return 0;
}
