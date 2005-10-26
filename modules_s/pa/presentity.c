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
		LOG(L_ERR, "new_presentity_no_wb(): Invalid parameter value\n");
		return -1;
	}

	size = sizeof(presentity_t) + _uri->len + 1;
	presentity = (presentity_t*)shm_malloc(size);
	if (!presentity) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_presentity_no_wb(): No memory left: size=%d\n", size);
		return -1;
	}
	memset(presentity, 0, sizeof(presentity_t));

	presentity->uri.s = ((char*)presentity) + sizeof(presentity_t);
	strncpy(presentity->uri.s, _uri->s, _uri->len);
	presentity->uri.s[_uri->len] = 0;
	presentity->uri.len = _uri->len;
	presentity->pdomain = pdomain;
	presentity->first_qsa_subscription = 0;
	presentity->last_qsa_subscription = 0;
	presentity->presid = 0;

	*_p = presentity;

	LOG(L_ERR, "new_presentity_no_wb=%p for uri=%.*s\n", 
			presentity, presentity->uri.len, presentity->uri.s);

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
		LOG(L_ERR, "new_presentity: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, query_cols, query_ops, query_vals, n_query_cols) < 0) {
		LOG(L_ERR, "new_presentity: Error while querying presentity\n");
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
		LOG(L_ERR, "new_presentity: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, query_cols, query_ops, query_vals, n_query_cols) < 0) {
		LOG(L_ERR, "new_presentity: Error while querying presentity\n");
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
	res = db_remove_watchers(presentity) | res;
	 
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
		LOG(L_ERR, "new_presentity: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, query_cols, query_ops, query_vals, n_query_cols) < 0) {
		LOG(L_ERR, "new_presentity: Error while querying presentity\n");
		return -1;
	}
	
	return res;
}
/*
 * Create a new presentity
 */
int new_presentity(struct pdomain *pdomain, str* _uri, presentity_t** _p)
{
	int res = 0;

	res = new_presentity_no_wb(pdomain, _uri, _p);
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



/*
 * Free all memory associated with a presentity
 */
void free_presentity(presentity_t* _p)
{
	watcher_t* ptr;
	presence_tuple_t *tuple;
	internal_pa_subscription_t *iw, *niw;

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

	shm_free(_p);
}

static int db_remove_presence_tuple(presentity_t *_p, presence_tuple_t *t)
{
	db_key_t keys[] = { "presid", "tupleid" };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[] = { { DB_INT, 0, { .int_val = _p->presid } },
		{ DB_STR, 0, { .str_val = t->id } } };
	
	if (!use_db) return 0;
	if (!t->is_published) return 0; /* store only published tuples */

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

	if (use_place_table) {
		int placeid = 0;
		LOG(L_ERR, "set_duple_db_data: room=%.*s loc=%.*s\n", 
				tuple->location.room.len, tuple->location.room.s,
				tuple->location.loc.len, tuple->location.loc.s);

		if (tuple->location.room.len && tuple->location.room.s) {
			location_lookup_placeid(&tuple->location.room, &placeid);
		} else if (tuple->location.loc.len && tuple->location.loc.s) {
			location_lookup_placeid(&tuple->location.loc, &placeid);
		}
		if (placeid) {
			cols[n_updates] = "placeid";
			vals[n_updates].type = DB_INT;
			vals[n_updates].nul = 0;
			vals[n_updates].val.int_val = placeid;
			n_updates++;
		}
	} else {
		if (tuple->location.loc.len && tuple->location.loc.s) {
			cols[n_updates] = "location";
			vals[n_updates].type = DB_STR;
			vals[n_updates].nul = 0;
			vals[n_updates].val.str_val = tuple->location.loc;
			LOG(L_ERR, "set_tuple_db_data:  tuple->location.loc=%s len=%d\n", 
					tuple->location.loc.s, tuple->location.loc.len);
			n_updates++;
		}
		if (tuple->location.site.len && tuple->location.site.s) {
			cols[n_updates] = "site";
			vals[n_updates].type = DB_STR;
			vals[n_updates].nul = 0;
			vals[n_updates].val.str_val = tuple->location.site;
			n_updates++;
		}
		if (tuple->location.floor.len && tuple->location.floor.s) {
			cols[n_updates] = "floor";
			vals[n_updates].type = DB_STR;
			vals[n_updates].nul = 0;
			vals[n_updates].val.str_val = tuple->location.floor;
			n_updates++;
		}
		if (tuple->location.room.len && tuple->location.room.s) {
			cols[n_updates] = "room";
			vals[n_updates].type = DB_STR;
			vals[n_updates].nul = 0;
			vals[n_updates].val.str_val = tuple->location.room;
			n_updates++;
		}
	}
	if (tuple->location.x != 0) {
		cols[n_updates] = "x";
		vals[n_updates].type = DB_DOUBLE;
		vals[n_updates].nul = 0;
		vals[n_updates].val.double_val = tuple->location.x;
		n_updates++;
	}
	if (tuple->location.y != 0) {
		cols[n_updates] = "y";
		vals[n_updates].type = DB_DOUBLE;
		vals[n_updates].nul = 0;
		vals[n_updates].val.double_val = tuple->location.y;
		n_updates++;
	}
	if (tuple->location.radius != 0) {
		cols[n_updates] = "radius";
		vals[n_updates].type = DB_DOUBLE;
		vals[n_updates].nul = 0;
		vals[n_updates].val.double_val = tuple->location.radius;
		n_updates++;
	}
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

int db_update_presence_tuple(presentity_t *_p, presence_tuple_t *t)
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
	
	return 0;
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
		"prescaps" } ;
	
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
		str contact = { 0, 0 }; 
		str basic = { 0, 0 }; 
		str status = { 0, 0 }; 
		str location = { 0, 0 }; 
		str id = { 0, 0 }; 
		
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
		
#undef get_str_val		
#undef get_time_val		

		r = new_presence_tuple(&contact, expires, _p, &tuple, 1) | r;
		if (tuple) {
			tuple->state = basic2status(basic);
			memcpy(tuple->status.s, status.s, status.len);
			tuple->status.len = status.len;
			LOG(L_ERR, "read tuple %.*s\n", id.len, id.s);
			memcpy(tuple->id.s, id.s, id.len);
			tuple->id.len = id.len;
			tuple->priority = priority;
			tuple->prescaps = prescaps;
			if (use_place_table) {
				/* FIXME: what to do with placeid ? */
			}
			else {
				/* FIXME: copy location, x, y, ... */
			}
			
			add_presence_tuple_no_wb(_p, tuple);
		}
	}
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
			db_update_presence_tuple(_p, tuple);
		}
	}
	return 0;
}


/*
 * Create a new presence_tuple
 */
int new_presence_tuple(str* _contact, time_t expires, presentity_t *_p, presence_tuple_t ** _t, int is_published)
{
	presence_tuple_t* tuple;
	int size = 0;

	if (!_contact || !_t) {
		paerrno = PA_INTERNAL_ERROR;
		LOG(L_ERR, "new_presence_tuple(): Invalid parameter value\n");
		return -1;
	}

	size = sizeof(presence_tuple_t) + _contact->len + 1;
	tuple = (presence_tuple_t*)shm_malloc(size);
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
	tuple->location.loc.s = tuple->location.loc_buf;
	tuple->location.site.s = tuple->location.site_buf;
	tuple->location.floor.s = tuple->location.floor_buf;
	tuple->location.room.s = tuple->location.room_buf;
	tuple->location.packet_loss.s = tuple->location.packet_loss_buf;
	tuple->id.s = tuple->id_buf;
	tuple->expires = expires;
	tuple->priority = default_priority;
	tuple->is_published = is_published;

	tuple->id.len = sprintf(tuple->id.s, "%px%xx%x", 
			tuple, rand(), (unsigned int)time(NULL));

	*_t = tuple;

	LOG(L_ERR, "new_tuple=%p for aor=%.*s contact=%.*s id=%.*s\n", tuple, 
			_p->uri.len, _p->uri.s,
			tuple->contact.len, tuple->contact.s,
			tuple->id.len, tuple->id.s);

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
	//LOG(L_ERR, "find_presence_tuple: _p=%p _p->tuples=%p\n", _p, _p->tuples);
	while (tuple) {
		if (str_strcasecmp(&tuple->contact, _contact) == 0) {
			*_t = tuple;
			return 0;
		}
		tuple = tuple->next;
	}
	return 1;
}

/*
 * Find a presence_tuple _contact on presentity _p
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
	/* FIXME: should be in new_tuple? */
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

static void process_watchers(presentity_t* _p, int *changed)
{
	watcher_t *next, *w, *prev;
	int presentity_changed;
	int notify;
	
	/* !!! "changed" is not initialized here it is only set if change
	 * in presentity occurs */
	
	presentity_changed = _p->flags & (PFLAG_PRESENCE_CHANGED
			| PFLAG_PRESENCE_LISTS_CHANGED
			| PFLAG_XCAP_CHANGED
			| PFLAG_LOCATION_CHANGED);

	prev = NULL;
	w = _p->watchers;
	while (w) {
		/* changes status of expired watcher */
		if (w->expires <= act_time) {
			LOG(L_ERR, "Expired watcher %.*s\n", w->uri.len, w->uri.s);
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
			if (changed) *changed = 1;
		}
		else {
			prev = w;
			w = w->next;
		}
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
			LOG(L_ERR, "Expired watcher %.*s\n", w->uri.len, w->uri.s);
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
			LOG(L_ERR, "Expiring tuple %.*s\n", t->contact.len, t->contact.s);
			remove_presence_tuple(_p, t);
			free_presence_tuple(t);
			if (changed) *changed = 1;
			_p->flags |= PFLAG_PRESENCE_CHANGED;
		}
		t = n;
	}
}

int timer_presentity(presentity_t* _p)
{
	int changed, old_flags;
	int presentity_changed;

	changed = 0;
	old_flags = _p->flags;

	/* mark expired tuples (or remove them ?) */
	/* mark_expired_tuples(_p, &changed); */
	remove_expired_tuples(_p, &changed); /* FIXME: use changed here is useless */
	
	/* notify watchers and remove expired */
	process_watchers(_p, &changed);	/* FIXME: use changed here is useless */
	/* notify winfo watchers and remove expired */
	process_winfo_watchers(_p, &changed); /* FIXME: use changed here is useless */
	
	/* notify internal watchers */
	presentity_changed = _p->flags & (PFLAG_PRESENCE_CHANGED
			|PFLAG_PRESENCE_LISTS_CHANGED
			|PFLAG_XCAP_CHANGED
			|PFLAG_LOCATION_CHANGED);
	if (presentity_changed) {
		/* DEBUG_LOG("presentity %.*s changed\n", _p->uri.len, _p->uri.s); */
		notify_qsa_watchers(_p);
	}

	/* clear presentity "change" flags */
	_p->flags &= ~(PFLAG_PRESENCE_CHANGED
			| PFLAG_PRESENCE_LISTS_CHANGED
			| PFLAG_XCAP_CHANGED
			| PFLAG_LOCATION_CHANGED
			| PFLAG_WATCHERINFO_CHANGED);
	if (_p->flags != old_flags) changed = 1; /* ??? */
	
	/* remove expired tuples */
	/* remove_expired_tuples(_p, &changed); */

	/* update DB record if something changed - USELESS */
/*	if (changed) {
		db_update_presentity(_p);
	} 
*/	
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
			
	LOG(L_ERR, "removing watcher %p (pres %p)\n", _w, _p);
	
	while(watcher) {
		if (watcher == _w) {
			if (prev) {
				prev->next = watcher->next;
			} else {
				_p->watchers = watcher->next;
			}
			LOG(L_ERR, "removed watcher %p (pres %p)\n", _w, _p);
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

/* returns 0 if equal strings */
/*static int str_case_equals(const str *a, const str *b)
{
	int i;

	if (!a) {
		if (!b) return 0;
		else return 1;
	}
	if (!b) return 1;
	if (a->len != b->len) return 1;

	for (i = 0; i < a->len; i++) 
		if (a->s[i] != b->s[i]) return 1;
	return 0;
}*/

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
	if (!use_db) return 0;

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
	presentity_t *presentity = NULL;
	db_con_t* db = create_pa_db_connection(); /* must create its own connection (called before child init)! */

	if (!db) {
		LOG(L_ERR, "Can't load presentities - no DB connection\n");
		return -1;
	}

	query_cols[n_query_cols] = "pdomain";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *pdomain->name;
	n_query_cols++;

	result_cols[uri_col = n_result_cols++] = "uri";
	result_cols[presid_col = n_result_cols++] = "presid";

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
			str uri;
			if (!row_vals[uri_col].nul) {
				uri.s = (char *)row_vals[uri_col].val.string_val;
				uri.len = strlen(uri.s);
			}

			LOG(L_INFO, "pdomain_load_presentities: pdomain=%.*s presentity uri=%.*s presid=%d\n",
					pdomain->name->len, pdomain->name->s, uri.len, uri.s, presid);

			new_presentity_no_wb(pdomain, &uri, &presentity);
			if (presentity) {
				presentity->presid = presid;
				add_presentity(pdomain, presentity);
			}
		}
		pa_dbf.free_result(db, res);
	}

	for (presentity = pdomain->first; presentity; presentity = presentity->next) {
		db_read_watcherinfo(presentity, db);
		db_read_tuples(presentity, db);
	}
	
	close_pa_db_connection(db);
	return 0;
}
