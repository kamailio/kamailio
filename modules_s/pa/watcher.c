/*
 * Presence Agent, watcher structure and related functions
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

extern db_con_t* pa_db;

char *doctype_name[] = {
	[DOC_XPIDF] = "DOC_XPIDF",
	[DOC_LPIDF] = "DOC_LPIDF",
	[DOC_PIDF] = "DOC_PIDF",
	[DOC_WINFO] = "DOC_WINFO",
	[DOC_XCAP_CHANGE] = "DOC_XCAP_CHANGE",
	[DOC_LOCATION] = "DOC_LOCATION"
};

char *event_package_name[] = {
	[EVENT_OTHER] = "unknown",
	[EVENT_PRESENCE] = "presence",
	[EVENT_PRESENCE_WINFO] = "presence.winfo",
	[EVENT_XCAP_CHANGE] = "xcap-change",
	[EVENT_LOCATION] = "location",
	NULL
};

static str watcher_status_names[] = {
     [WS_PENDING] = { "pending", 7 },
     [WS_ACTIVE] = { "active", 6 },
     [WS_WAITING] = { "waiting", 7 },
     [WS_TERMINATED] = { "terminated", 10 },
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

int event_package_from_string(str *epname) 
{
     int i;
     for (i = 0; event_package_name[i]; i++) {
	  if (strcasecmp(epname->s, event_package_name[i]) == 0) {
	       return i;
	  }
     }
     return 0;
}

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

static char hbuf[2048];

static int watcher_assign_statement_id(presentity_t *presentity, watcher_t *watcher)
{
	unsigned int h = 0;
	char *dn = doctype_name[watcher->accept];
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
	watcher->s_id.len = sprintf(watcher->s_id.s, "SID%08x", h);
	return 0;
}

/*
 * Create a new watcher structure but do not write to database
 */
int new_watcher_no_wb(presentity_t *_p, str* _uri, time_t _e, int event_package, doctype_t _a, dlg_t* _dlg, 
		      str *_dn, watcher_t** _w)
{
	watcher_t* watcher;

	/* Check parameters */
	if (!_uri && !_dlg && !_w) {
		LOG(L_ERR, "new_watcher(): Invalid parameter value\n");
		return -1;
	}

	/* Allocate memory buffer for watcher_t structure and uri string */
	watcher = (watcher_t*)shm_malloc(sizeof(watcher_t) + _uri->len + _dn->len + S_ID_LEN);
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

	watcher->s_id.s = (char*)watcher + sizeof(watcher_t);
	watcher->s_id.len = 0;

	watcher->event_package = event_package;
	watcher->expires = _e; /* Expires value */
	watcher->accept = _a;  /* Accepted document type */
	watcher->dialog = _dlg; /* Dialog handle */
	watcher->event = WE_SUBSCRIBE;
	*_w = watcher;

	return 0;
}

/*
 * Create a new watcher structure
 */
int new_watcher(presentity_t *_p, str* _uri, time_t _e, int event_package, doctype_t _a, dlg_t* _dlg, 
		str *_dn, watcher_t** _w)
{
     int rc;
     watcher_t* watcher;

     /* Check parameters */
     if (!_uri && !_dlg && !_w) {
	  LOG(L_ERR, "new_watcher(): Invalid parameter value\n");
	  return -1;
     }

     rc = new_watcher_no_wb(_p, _uri, _e, event_package, _a, _dlg, _dn, _w);
     if (rc < 0) {
	  return rc;
     } else {
	  watcher = *_w;
     }

     if (use_db) {
	  db_key_t query_cols[11];
	  db_op_t query_ops[11];
	  db_val_t query_vals[11];

	  db_key_t result_cols[5];
	  db_res_t *res;
	  int n_query_cols = 0;
	  int n_result_cols = 0;
	  int s_id_col, status_col, display_name_col, event_col;
	  char *package = event_package_name[watcher->event_package];

	  LOG(L_ERR, "db_new_watcher starting\n");
	  query_cols[n_query_cols] = "r_uri";
	  query_ops[n_query_cols] = OP_EQ;
	  query_vals[n_query_cols].type = DB_STR;
	  query_vals[n_query_cols].nul = n_query_cols;
	  query_vals[n_query_cols].val.str_val.s = _p->uri.s;
	  query_vals[n_query_cols].val.str_val.len = _p->uri.len;
	  n_query_cols++;
	  LOG(L_ERR, "db_new_watcher:  _p->uri=%.*s\n", _p->uri.len, _p->uri.s);

	  query_cols[n_query_cols] = "w_uri";
	  query_ops[n_query_cols] = OP_EQ;
	  query_vals[n_query_cols].type = DB_STR;
	  query_vals[n_query_cols].nul = 0;
	  query_vals[n_query_cols].val.str_val.s = watcher->uri.s;
	  query_vals[n_query_cols].val.str_val.len = watcher->uri.len;
	  n_query_cols++;
	  LOG(L_ERR, "db_new_watcher:  watcher->uri=%.*s\n", watcher->uri.len, watcher->uri.s);


	  query_cols[n_query_cols] = "package";
	  query_ops[n_query_cols] = OP_EQ;
	  query_vals[n_query_cols].type = DB_STR;
	  query_vals[n_query_cols].nul = 0;
	  query_vals[n_query_cols].val.str_val.s = package;
	  query_vals[n_query_cols].val.str_val.len = strlen(package);
	  n_query_cols++;
	  LOG(L_ERR, "db_new_watcher:  watcher->package=%s\n", package);

	  result_cols[s_id_col = n_result_cols++] = "s_id";
	  result_cols[status_col = n_result_cols++] = "status";
	  result_cols[event_col = n_result_cols++] = "event";
	  result_cols[display_name_col = n_result_cols++] = "display_name";
		
	  db_use_table(pa_db, watcherinfo_table);
	  if (db_query (pa_db, query_cols, query_ops, query_vals,
			result_cols, n_query_cols, n_result_cols, 0, &res) < 0) {
	       LOG(L_ERR, "new_watcher(): Error while querying tuple\n");
	       return -1;
	  }
	  LOG(L_INFO, "new_watcher: getting values: res=%p res->n=%d\n",
	      res, (res ? res->n : 0));
	  if (res && res->n > 0) {
	       /* fill in tuple structure from database query result */
	       db_row_t *row = &res->rows[0];
	       db_val_t *row_vals = ROW_VALUES(row);
	       str s_id = { 0, 0 };
	       str status = { 0, 0 };
	       str event_str = { 0, 0 };
	       watcher_event_t watcher_event = WE_SUBSCRIBE;
	       if (!row_vals[s_id_col].nul) {
		    s_id.s = row_vals[s_id_col].val.string_val;
		    s_id.len = strlen(s_id.s);
	       }
	       if (!row_vals[status_col].nul) {
		    status.s = row_vals[status_col].val.string_val;
		    status.len = strlen(status.s);
	       }
	       if (!row_vals[event_col].nul) {
		    event_str.s = row_vals[event_col].val.string_val;
		    event_str.len = strlen(event_str.s);
		    watcher_event = watcher_event_from_string(&event_str);
	       }
	       watcher->event = watcher_event;
	       
	       LOG(L_ERR, "new_watcher: status=%.*s\n", status.len, status.s);
	       if (status.len) {
		    watcher->status = watcher_status_from_string(&status);
	       } else {
		    watcher->status = WS_ACTIVE;
	       }
	       if (s_id.s) {
		    strncpy(watcher->s_id.s, s_id.s, S_ID_LEN);
		    watcher->s_id.len = strlen(s_id.s);
	       }
	  } else {

	       watcher_assign_statement_id(_p, watcher);

	       query_cols[n_query_cols] = "s_id";
	       query_vals[n_query_cols].type = DB_STR;
	       query_vals[n_query_cols].nul = 0;
	       query_vals[n_query_cols].val.str_val.s = watcher->s_id.s;
	       query_vals[n_query_cols].val.str_val.len = watcher->s_id.len;
	       n_query_cols++;

	       query_cols[n_query_cols] = "status";
	       query_vals[n_query_cols].type = DB_STR;
	       query_vals[n_query_cols].nul = 0;
	       if (new_watcher_pending) {
		    query_vals[n_query_cols].val.str_val.s = "pending";
		    query_vals[n_query_cols].val.str_val.len = strlen("pending");
	       } else {
		    query_vals[n_query_cols].val.str_val.s = "active";
		    query_vals[n_query_cols].val.str_val.len = strlen("active");
	       }
	       n_query_cols++;

	       query_cols[n_query_cols] = "event";
	       query_vals[n_query_cols].type = DB_STR;
	       query_vals[n_query_cols].nul = 0;
	       query_vals[n_query_cols].val.str_val = watcher_event_names[watcher->event];
	       n_query_cols++;

	       query_cols[n_query_cols] = "display_name";
	       query_vals[n_query_cols].type = DB_STR;
	       query_vals[n_query_cols].nul = 0;
	       query_vals[n_query_cols].val.str_val.s = watcher->display_name.s;
	       query_vals[n_query_cols].val.str_val.len = watcher->display_name.len;
	       n_query_cols++;

	       query_cols[n_query_cols] = "accepts";
	       query_vals[n_query_cols].type = DB_INT;
	       query_vals[n_query_cols].nul = 0;
	       query_vals[n_query_cols].val.int_val = watcher->accept;
	       n_query_cols++;

	       query_cols[n_query_cols] = "expires";
	       query_vals[n_query_cols].type = DB_INT;
	       query_vals[n_query_cols].nul = 0;
	       query_vals[n_query_cols].val.int_val = watcher->expires;
	       n_query_cols++;

	       /* insert new record into database */
	       LOG(L_INFO, "new_tuple: inserting %d cols into table\n", n_query_cols);
	       if (db_insert(pa_db, query_cols, query_vals, n_query_cols) < 0) {
		    LOG(L_ERR, "db_new_tuple(): Error while inserting tuple\n");
		    return -1;
	       }
	  }
	  if (res)
	       db_free_query(pa_db, res);
     }

     return 0;
}


/*
 * Read watcherinfo table from database for presentity _p
 */
int db_read_watcherinfo(presentity_t *_p)
{
     if (use_db) {
	  db_key_t query_cols[5];
	  db_op_t query_ops[5];
	  db_val_t query_vals[5];

	  db_key_t result_cols[9];
	  db_res_t *res;
	  int n_query_cols = 1;
	  int n_result_cols = 0;
	  int w_uri_col, s_id_col, event_package_col, status_col, watcher_event_col, display_name_col, accepts_col, expires_col;

	  // LOG(L_ERR, "db_read_watcherinfo starting\n");
	  query_cols[0] = "r_uri";
	  query_ops[0] = OP_EQ;
	  query_vals[0].type = DB_STRING;
	  query_vals[0].nul = 0;
	  query_vals[0].val.string_val = _p->uri.s;
	  LOG(L_ERR, "db_read_watcherinfo:  _p->uri='%s'\n", _p->uri.s);

	  result_cols[w_uri_col = n_result_cols++] = "w_uri";
	  result_cols[s_id_col = n_result_cols++] = "s_id";
	  result_cols[event_package_col = n_result_cols++] = "package";
	  result_cols[status_col = n_result_cols++] = "status";
	  result_cols[display_name_col = n_result_cols++] = "display_name";
	  result_cols[accepts_col = n_result_cols++] = "accepts";
	  result_cols[expires_col = n_result_cols++] = "expires";
	  result_cols[watcher_event_col = n_result_cols++] = "event";
		
	  db_use_table(pa_db, watcherinfo_table);
	  if (db_query (pa_db, query_cols, query_ops, query_vals,
			result_cols, n_query_cols, n_result_cols, 0, &res) < 0) {
	       LOG(L_ERR, "db_read_watcherinfo(): Error while querying watcherinfo\n");
	       return -1;
	  }
	  if (res && res->n > 0) {
	       /* fill in tuple structure from database query result */
	       int i;
	       for (i = 0; i < res->n; i++) {
		    db_row_t *row = &res->rows[i];
		    db_val_t *row_vals = ROW_VALUES(row);
		    str w_uri = { 0, 0 };
		    str s_id = { 0, 0 };
		    str event_package_str = { 0, 0 };
		    int event_package = EVENT_PRESENCE;
		    str watcher_event_str = { 0, 0 };
		    watcher_event_t watcher_event = WE_SUBSCRIBE;
		    int accepts = row_vals[accepts_col].val.int_val;
		    int expires = row_vals[expires_col].val.int_val;
		    str status = { 0, 0 };
		    str display_name = { 0, 0 };
		    watcher_t *watcher = NULL;
		    if (!row_vals[w_uri_col].nul) {
			 w_uri.s = row_vals[w_uri_col].val.string_val;
			 w_uri.len = strlen(w_uri.s);
		    }
		    if (!row_vals[s_id_col].nul) {
			 s_id.s = row_vals[s_id_col].val.string_val;
			 s_id.len = strlen(s_id.s);
		    }
		    if (!row_vals[event_package_col].nul) {
			 event_package_str.s = row_vals[event_package_col].val.string_val;
			 event_package_str.len = strlen(event_package_str.s);
			 event_package = event_package_from_string(&event_package_str);
		    }
		    if (!row_vals[status_col].nul) {
			 status.s = row_vals[status_col].val.string_val;
			 status.len = strlen(status.s);
		    }
		    if (!row_vals[watcher_event_col].nul) {
			 watcher_event_str.s = row_vals[watcher_event_col].val.string_val;
			 watcher_event_str.len = strlen(watcher_event_str.s);
			 watcher_event = watcher_event_from_string(&watcher_event_str);
		    }
		    if (!row_vals[display_name_col].nul) {
			 display_name.s = row_vals[display_name_col].val.string_val;
			 display_name.len = strlen(display_name.s);
		    }

		    if (find_watcher(_p, &w_uri, event_package, &watcher) != 0) {
			 new_watcher_no_wb(_p, &w_uri, expires, event_package, accepts, NULL, &display_name, &watcher);
		    }
		    if (watcher) {
			 watcher_status_t ws = watcher_status_from_string(&status);
			 if (watcher->status != ws)
			      watcher->flags |= WFLAG_SUBSCRIPTION_CHANGED;
			 watcher->status = ws;
			 watcher->event = watcher_event;
			     
			 if (s_id.s) {
			      strncpy(watcher->s_id.s, s_id.s, S_ID_LEN);
			      watcher->s_id.len = strlen(s_id.s);
			 }
		    }
	       }
	  }
	  db_free_query(pa_db, res);
	  LOG(L_ERR, "db_read_watcherinfo:  _p->uri='%s' done\n", _p->uri.s);
     }
     return 0;
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
	fprintf(_f, "accept : %s\n", doctype_name[_w->accept]);
	fprintf(_f, "next   : %p\n", _w->next);
	tmb.print_dlg(_f, _w->dialog);
	fprintf(_f, "---/Watcher---\n");
}


/*
 * Update a watcher structure
 */
int update_watcher(watcher_t* _w, time_t _e)
{
	_w->expires = _e;
	return 0;
}

#define CRLF "\r\n"
#define CRLF_L (sizeof(CRLF) - 1)

#define PUBLIC_ID "//IETF//DTD RFCxxxx PIDF 1.0//EN"
#define PUBLIC_ID_L (sizeof(PUBLIC_ID) - 1)

#define XML_VERSION "<?xml version=\"1.0\"?>"
#define XML_VERSION_L (sizeof(XML_VERSION) - 1)

#define WATCHERINFO_STAG "<watcherinfo xmlns=\"urn:ietf:params:xml:ns:watcherinfo\" version=\"0\" state=\"partial\">"
#define WATCHERINFO_STAG_L (sizeof(WATCHERINFO_STAG) - 1)
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

#define add_string(_s, _l) ((strs[n_strs].s = (_s)), (strs[n_strs].len = (_l)), (len += _l), n_strs++)
#define add_str(_s) (strs[n_strs].s = (_s.s), strs[n_strs].len = (_s.len), len += _s.len, n_strs++)
#define add_pstr(_s) (strs[n_strs].s = (_s->s), strs[n_strs].len = (_s->len), len += _s->len, n_strs++)

	add_string(WATCHER_START, WATCHER_START_L);
	add_string(STATUS_START, STATUS_START_L);
	add_str(watcher_status_names[status]);
	add_string(EVENT_START, EVENT_START_L);
	add_str(watcher_event_names[watcher->event]);
	add_string(SID_START, SID_START_L);
	add_str(watcher->s_id);
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
 * Create start of winfo document
 */
int start_winfo_doc(str* _b, int _l)
{
	str strs[10];
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

	add_string(XML_VERSION, XML_VERSION_L);
	add_string(CRLF, CRLF_L);
	add_string(WATCHERINFO_STAG, WATCHERINFO_STAG_L);
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

	add_string(WATCHERLIST_START, WATCHERLIST_START_L);
	add_pstr(_uri);
	add_string(PACKAGE_START, PACKAGE_START_L);
	add_string(event_package_name[watcher->event_package], strlen(event_package_name[watcher->event_package]));
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
