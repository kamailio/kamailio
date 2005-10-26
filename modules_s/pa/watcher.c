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

char *doctype_name[] = {
	[DOC_XPIDF] = "DOC_XPIDF",
	[DOC_LPIDF] = "DOC_LPIDF",
	[DOC_PIDF] = "DOC_PIDF",
#ifdef SUBTYPE_XML_MSRTC_PIDF
	[DOC_MSRTC_PIDF] = "DOC_MSRTC_PIDF",
#endif
	[DOC_WINFO] = "DOC_WINFO",
#ifdef DOC_XCAP_CHANGE
	[DOC_XCAP_CHANGE] = "DOC_XCAP_CHANGE",
#endif
#ifdef DOC_LOCATION
	[DOC_LOCATION] = "DOC_LOCATION"
#endif
};
/*
char *event_package_name[] = {
	[EVENT_OTHER] = "unknown",
	[EVENT_PRESENCE] = "presence",
	[EVENT_PRESENCE_WINFO] = "presence.winfo",
#ifdef DOC_XCAP_CHANGE
	[EVENT_XCAP_CHANGE] = "xcap-change",
#endif
#ifdef DOC_LOCATION
	[EVENT_LOCATION] = "location",
#endif
	NULL
};*/

str watcher_status_names[] = {
     [WS_PENDING] = { "pending", 7 },
     [WS_ACTIVE] = { "active", 6 },
     [WS_REJECTED] = { "rejected", 8 },
     [WS_TERMINATED] = { "terminated", 10 },
     [WS_PENDING_TERMINATED] = { "terminated", 10 },
     { 0, 0 }
};

static str watcher_event_names[] = {
     [WE_SUBSCRIBE]   = { "subscribe",   9 },
     [WE_APPROVED]    = { "approved",    8 },
     [WE_DEACTIVATED] = { "deactivated", 11 },
     [WE_PROBATION]   = { "probation",   9 },
     [WE_REJECTED]    = { "rejected",    8 },
     [WE_TIMEOUT]     = { "timeout",     7 },
     [WE_GIVEUP]      = { "giveup",      6 },
     [WE_NORESOURCE]  = { "noresource",  10 },
     { 0, 0 }
};

static const char *event_package2str(int et) /* FIXME: change working with this to enum ?*/
{
	/* added due to incorrect package handling */
	switch (et) {
		case EVENT_PRESENCE: return "presence";
		case EVENT_PRESENCE_WINFO: return "presence.winfo";
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

/* returns 1 if package supported by PA */
int verify_event_package(int et)
{
	switch (et) {
		case EVENT_PRESENCE:
		case EVENT_PRESENCE_WINFO: return 0;
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

/* static char hbuf[2048]; */

static int watcher_assign_statement_id(presentity_t *presentity, watcher_t *watcher)
{
	/*unsigned int h = 0;
	char *dn = doctype_name[watcher->preferred_mimetype];
	if (1) {
		int len = 0;
		strncpy(hbuf+len, presentity->uri.s, presentity->uri.len);
		len += presentity->uri.len;
		strncpy(hbuf+len, dn, strlen(dn));
		len += strlen(dn);
		strncpy(hbuf+len, watcher->uri.s, watcher->uri.len);
		len += watcher->uri.len;
		h = compute_hash(0, hbuf, len);
	} else {
		h = compute_hash(0, presentity->uri.s, presentity->uri.len);
		h = compute_hash(h, dn, strlen(dn));
		h = compute_hash(h, watcher->uri.s, watcher->uri.len);
	}
	watcher->s_id.len = sprintf(watcher->s_id.s, "SID%08x", h);*/
	watcher->s_id.len = sprintf(watcher->s_id.s, "SID%dx%px%dx%d", 
			presentity->presid, watcher, (int)time(NULL), rand());
	return 0;
}

/*
 * Create a new watcher structure but do not write to database
 */
int new_watcher_no_wb(presentity_t *_p, str* _uri, time_t _e, int event_package, doctype_t _a, dlg_t* _dlg, 
		      str *_dn, str *server_contact, watcher_t** _w)
{
	watcher_t* watcher;

	/* Check parameters */
	if (!_uri && !_dlg && !_w) {
		LOG(L_ERR, "new_watcher(): Invalid parameter value\n");
		return -1;
	}

	/* Allocate memory buffer for watcher_t structure and uri string */
	watcher = (watcher_t*)shm_malloc(sizeof(watcher_t) + _uri->len + _dn->len + S_ID_LEN + server_contact->len);
	if (!watcher) {
		paerrno = PA_NO_MEMORY;
	        LOG(L_ERR, "new_watcher(): No memory left\n");
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
	watcher->preferred_mimetype = _a;  /* Accepted document type */
	watcher->dialog = _dlg; /* Dialog handle */
	watcher->event = WE_SUBSCRIBE;
	watcher->status = WS_PENDING;
	
	*_w = watcher;

	return 0;
}

static int set_watcher_db_data(presentity_t *_p, watcher_t *watcher,
		db_key_t *cols, db_val_t *vals, int *col_cnt)
{
	int n_cols = 0;
	char *package = (char*)event_package2str(watcher->event_package);
	str dialog; /* serialized dialog */

	if (dlg_func.dlg2str(watcher->dialog, &dialog) != 0) {	
		LOG(L_ERR, "Error while serializing dialog\n");
		return -1;
	}
	
	cols[n_cols] = "r_uri";
	vals[n_cols].type = DB_STR;
	vals[n_cols].nul = n_cols;
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

	return 0;
}

int db_add_watcher(presentity_t *_p, watcher_t *watcher)
{
	db_key_t query_cols[20];
	db_val_t query_vals[20];
	
	int n_query_cols = 0;

	if (!use_db) return 0;
	
	if (watcher->s_id.len < 1) /* id not assigned yet */
		watcher_assign_statement_id(_p, watcher);
	
	if (set_watcher_db_data(_p, watcher, 
				query_cols, query_vals, &n_query_cols) != 0) {
		return -1;
	}

	if (pa_dbf.use_table(pa_db, watcherinfo_table) < 0) {
		LOG(L_ERR, "db_add_watcher: Error in use_table\n");
		return -1;
	}
	
	/* insert new record into database */
	if (pa_dbf.insert(pa_db, query_cols, query_vals, n_query_cols) < 0) {
		LOG(L_ERR, "db_add_watcher: Error while inserting watcher\n");
		return -1;
	}
	
	return 0;
}

int db_update_watcher(presentity_t *_p, watcher_t *watcher)
{
	db_key_t query_cols[20];
	db_val_t query_vals[20];
	int n_query_cols = 0;
	
	db_key_t keys[] = { "s_id" };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { { DB_STR, 0, { .str_val = watcher->s_id } } };

	if (!use_db) return 0;

	if (set_watcher_db_data(_p, watcher, 
				query_cols, query_vals, &n_query_cols) != 0) {
		return -1;
	}

	if (pa_dbf.use_table(pa_db, watcherinfo_table) < 0) {
		LOG(L_ERR, "db_update_watcher: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.update(pa_db, keys, ops, k_vals, 
				query_cols, query_vals, 1, n_query_cols) < 0) {
		LOG(L_ERR, "Error while updating watcher in DB\n");
		return -1;
	}

	return 0;
}

int db_remove_watcher(struct presentity *_p, watcher_t *w)
{
	db_key_t keys[] = { "s_id" };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { { DB_STR, 0, { .str_val = w->s_id } } };

	if (!use_db) return 0;
	
	if (pa_dbf.use_table(pa_db, watcherinfo_table) < 0) {
		LOG(L_ERR, "db_update_watcher: Error in use_table\n");
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

	str dialog = { 0, 0 };
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
	LOG(L_ERR, "db_read_watcherinfo:  _p->uri='%s', presid=%d\n", 
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
			str w_uri = { 0, 0 };
			str s_id = { 0, 0 };
			char *event_package_str = NULL;
			int event_package = EVENT_PRESENCE;
			str watcher_event_str = { 0, 0 };
			watcher_event_t watcher_event = WE_SUBSCRIBE;
			int accepts = row_vals[accepts_col].val.int_val;
			int expires = row_vals[expires_col].val.int_val;
			int doc_index = row_vals[doc_index_col].val.int_val;
			str status = { 0, 0 };
			str display_name = { 0, 0 };
			str server_contact = { 0, 0};
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
				dlg = (dlg_t*)shm_malloc(sizeof(*dlg));
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
	LOG(L_ERR, "db_read_watcherinfo:  _p->uri='%s' done\n", _p->uri.s);

	return r;
}

/*
 * Release a watcher structure
 */
void free_watcher(watcher_t* _w)
{
	tmb.free_dlg(_w->dialog);
	shm_free(_w);	
}


/*
 * Print contact, for debugging purposes only
 */
void print_watcher(FILE* _f, watcher_t* _w)
{
	fprintf(_f, "---Watcher---\n");
	fprintf(_f, "uri    : '%.*s'\n", _w->uri.len, ZSW(_w->uri.s));
	fprintf(_f, "expires: %d\n", (int)(_w->expires - time(0)));
	fprintf(_f, "accept : %s\n", doctype_name[_w->preferred_mimetype]);
	fprintf(_f, "next   : %p\n", _w->next);
	tmb.print_dlg(_f, _w->dialog);
	fprintf(_f, "---/Watcher---\n");
}


/*
 * Update a watcher structure
 */
int update_watcher(struct presentity *p, watcher_t* _w, time_t _e)
{
	_w->expires = _e;
	if (use_db) return db_update_watcher(p, _w);
	else return 0;
}

#define CRLF "\r\n"
#define CRLF_L (sizeof(CRLF) - 1)

#define PUBLIC_ID "//IETF//DTD RFCxxxx PIDF 1.0//EN"
#define PUBLIC_ID_L (sizeof(PUBLIC_ID) - 1)

#define XML_VERSION "<?xml version=\"1.0\"?>"
#define XML_VERSION_L (sizeof(XML_VERSION) - 1)

#define WATCHERINFO_STAG_A "<watcherinfo xmlns=\"urn:ietf:params:xml:ns:watcherinfo\" version=\""
#define WATCHERINFO_STAG_A_L (sizeof(WATCHERINFO_STAG_A) - 1)
#define WATCHERINFO_STAG_B "\" state=\"full\">"
#define WATCHERINFO_STAG_B_L (sizeof(WATCHERINFO_STAG_B) - 1)
#define WATCHERINFO_ETAG "</watcherinfo>"
#define WATCHERINFO_ETAG_L (sizeof(WATCHERINFO_ETAG) - 1)

#define WATCHERLIST_START "  <watcher-list resource=\"sip:"
#define WATCHERLIST_START_L (sizeof(WATCHERLIST_START) - 1)
#define WATCHERLIST_ETAG "  </watcher-list>"
#define WATCHERLIST_ETAG_L (sizeof(WATCHERLIST_ETAG) - 1)

#define WATCHER_START "    <watcher"
#define WATCHER_START_L (sizeof(WATCHER_START) - 1)
#define STATUS_START " status=\""
#define STATUS_START_L (sizeof(STATUS_START) - 1)
#define EVENT_START "\" event=\""
#define EVENT_START_L (sizeof(EVENT_START) - 1)
#define SID_START "\" id=\""
#define SID_START_L (sizeof(SID_START) - 1)
#define DISPLAY_NAME_START "\" display_name=\""
#define DISPLAY_NAME_START_L (sizeof(DISPLAY_NAME_START) - 1)
#define URI_START "\">"
#define URI_START_L (sizeof(URI_START) - 1)
#define WATCHER_ETAG "</watcher>"
#define WATCHER_ETAG_L (sizeof(WATCHER_ETAG) - 1)

#define PACKAGE_START "\" package=\""
#define PACKAGE_START_L (sizeof(PACKAGE_START) - 1)
#define PACKAGE_END "\">"
#define PACKAGE_END_L (sizeof(PACKAGE_END) - 1)

void escape_str(str *unescaped)
{
     int i;
     char *s = unescaped->s;
     for (i = 0; i < unescaped->len; i++) {
	  if (s[i] == '<' || s[i] == '>') {
	       s[i] = ' ';
	  }
     }
}

/*
 * Add a watcher information
 */
int winfo_add_watcher(str* _b, int _l, watcher_t *watcher)
{
	str strs[20];
	int n_strs = 0;
	int i;
	int len = 0;
	int status = watcher->status;
	char id[64];

#define add_string(_s, _l) ((strs[n_strs].s = (_s)), (strs[n_strs].len = (_l)), (len += _l), n_strs++)
#define add_str(_s) (strs[n_strs].s = (_s.s), strs[n_strs].len = (_s.len), len += _s.len, n_strs++)
#define add_pstr(_s) (strs[n_strs].s = (_s->s), strs[n_strs].len = (_s->len), len += _s->len, n_strs++)

	add_string(WATCHER_START, WATCHER_START_L);
	add_string(STATUS_START, STATUS_START_L);
	add_str(watcher_status_names[status]);
	add_string(EVENT_START, EVENT_START_L);
	add_str(watcher_event_names[watcher->event]);
	add_string(SID_START, SID_START_L);
	if (watcher->s_id.len < 1) {
		sprintf(id, "%p", watcher);
		add_string(id, strlen(id));
	}
	else add_str(watcher->s_id);
	if (0)
	if (watcher->display_name.len > 0) {
	  add_string(DISPLAY_NAME_START, DISPLAY_NAME_START_L);
	  escape_str(&watcher->display_name);
	  add_str(watcher->display_name);
	}
	add_string(URI_START, URI_START_L);
	add_str(watcher->uri);
	add_string(WATCHER_ETAG, WATCHER_ETAG_L);
	add_string(CRLF, CRLF_L);

	if (_l < len) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "winfo_add_watcher(): Buffer too small\n");
		return -1;
	}

	for (i = 0; i < n_strs; i++)
		str_append(_b, strs[i].s, strs[i].len);

	return 0;
}

/*
 * Add an internal watcher information
 */
int winfo_add_internal_watcher(str* _b, int _l, internal_pa_subscription_t *iwatcher)
{
	str strs[20];
	int n_strs = 0;
	int i;
	int len = 0;
	char id[64];

	sprintf(id, "%p", iwatcher);
	add_string(WATCHER_START, WATCHER_START_L);
	add_string(STATUS_START, STATUS_START_L);
	add_str(watcher_status_names[0]); /* TODO: authorization and status remeber ! */
	add_string(EVENT_START, EVENT_START_L);
	add_str(watcher_event_names[0]); /* TODO: auth */
	add_string(SID_START, SID_START_L);
	add_string(id, strlen(id));
	add_string(URI_START, URI_START_L);
	add_string(iwatcher->subscription->subscriber_id.s, 
			iwatcher->subscription->subscriber_id.len);
	add_string(WATCHER_ETAG, WATCHER_ETAG_L);
	add_string(CRLF, CRLF_L);

	if (_l < len) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "winfo_add_watcher(): Buffer too small\n");
		return -1;
	}

	for (i = 0; i < n_strs; i++)
		str_append(_b, strs[i].s, strs[i].len);

	return 0;
}


/*
 * Create start of winfo document
 */
int start_winfo_doc(str* _b, int _l, struct watcher *w)
{
	str strs[10];
	char version[32];
	int n_strs = 0;
	int i;
	int len = 0;

	if ((XML_VERSION_L + 
	     CRLF_L
	    ) > _l) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "start_pidf_doc(): Buffer too small\n");
		return -1;
	}
	if (w) sprintf(version, "%d", w->document_index++);
	else strcpy(version, "0");
	add_string(XML_VERSION, XML_VERSION_L);
	add_string(CRLF, CRLF_L);
	add_string(WATCHERINFO_STAG_A, WATCHERINFO_STAG_A_L);
	add_string(version, strlen(version));
	add_string(WATCHERINFO_STAG_B, WATCHERINFO_STAG_B_L);
	add_string(CRLF, CRLF_L);

	if (_l < len) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "winfo_add_resource(): Buffer too small\n");
		return -1;
	}

	for (i = 0; i < n_strs; i++)
		str_append(_b, strs[i].s, strs[i].len);

	return 0;
}

/*
 * Start a resource in a winfo document
 */
int winfo_start_resource(str* _b, int _l, str* _uri, watcher_t *watcher)
{
	str strs[10];
	int n_strs = 0;
	int i;
	int len = 0;
	char *package;

	package = (char*)event_package2str(EVENT_PRESENCE);
	add_string(WATCHERLIST_START, WATCHERLIST_START_L);
	add_pstr(_uri);
	add_string(PACKAGE_START, PACKAGE_START_L);
/*	add_string(event_package_name[watcher->event_package], strlen(event_package_name[watcher->event_package]));*/
	add_string(package, strlen(package)); /* FIXME */
	add_string(PACKAGE_END, PACKAGE_END_L);
	add_string(CRLF, CRLF_L);

	if (_l < len) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "winfo_add_resource(): Buffer too small\n");
		return -1;
	}

	for (i = 0; i < n_strs; i++)
		str_append(_b, strs[i].s, strs[i].len);

	return 0;
}

/*
 * End a resource in a winfo document
 */
int winfo_end_resource(str *_b, int _l)
{
	str strs[10];
	int n_strs = 0;
	int i;
	int len = 0;

	add_string(WATCHERLIST_ETAG, WATCHERLIST_ETAG_L);
	add_string(CRLF, CRLF_L);

	if (_l < len) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "winfo_add_resource(): Buffer too small\n");
		return -1;
	}

	for (i = 0; i < n_strs; i++)
		str_append(_b, strs[i].s, strs[i].len);

	return 0;
}

/*
 * End a winfo document
 */
int end_winfo_doc(str* _b, int _l)
{
	if (_l < (WATCHERINFO_ETAG_L + CRLF_L)) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "end_pidf_doc(): Buffer too small\n");
		return -1;
	}

	str_append(_b, WATCHERINFO_ETAG CRLF, WATCHERINFO_ETAG_L + CRLF_L);
	return 0;
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
