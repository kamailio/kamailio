/*
 * Presence Agent, location package handling
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 * Copyright (C) 2003-2004 Hewlett-Packard Company
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
 *
 */

#include <string.h>
#include "../../fifo_server.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_event.h"
#include "../../db/db.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "dlist.h"
#include "presentity.h"
#include "watcher.h"
#include "pstate.h"
#include "notify.h"
#include "paerrno.h"
#include "pdomain.h"
#include "pa_mod.h"
#include "ptime.h"
#include "reply.h"
#include "subscribe.h"
#include "publish.h"
#include "common.h"
#include "pa_mod.h"

#define CRLF "\r\n"
#define CRLF_L (sizeof(CRLF) - 1)

#define PUBLIC_ID "//IETF//DTD RFCxxxx PIDF 1.0//EN"
#define PUBLIC_ID_L (sizeof(PUBLIC_ID) - 1)

#define XML_VERSION "<?xml version=\"1.0\"?>"
#define XML_VERSION_L (sizeof(XML_VERSION) - 1)

#define LOCATIONINFO_STAG "<locationinfo xmlns=\"urn:hplabs:params:xml:ns:locationinfo\" version=\"0\" state=\"full\">"
#define LOCATIONINFO_STAG_L (sizeof(LOCATIONINFO_STAG) - 1)
#define LOCATIONINFO_ETAG "</locationinfo>"
#define LOCATIONINFO_ETAG_L (sizeof(LOCATIONINFO_ETAG) - 1)

#define USERLIST_START "  <user-list resource=\""
#define USERLIST_START_L (sizeof(USERLIST_START) - 1)
#define USERLIST_ETAG "  </user-list>"
#define USERLIST_ETAG_L (sizeof(USERLIST_ETAG) - 1)

#define USER_START "<user entity=\""
#define USER_START_L (sizeof(USER_START) - 1)
#define USER_END "\">"
#define USER_END_L (sizeof(USER_END) - 1)
#define USER_ETAG "</user>"
#define USER_ETAG_L (sizeof(USER_ETAG) - 1)

#define add_string(_s, _l) ((strs[n_strs].s = (_s)), (strs[n_strs].len = (_l)), (len += _l), n_strs++)
#define add_str(_s) (strs[n_strs].s = (_s.s), strs[n_strs].len = (_s.len), len += _s.len, n_strs++)
#define add_pstr(_s) (strs[n_strs].s = (_s->s), strs[n_strs].len = (_s->len), len += _s->len, n_strs++)


/*
 * Add a user information
 */
int location_doc_add_user(str* _b, int _l, str *user_uri)
{
	str strs[10];
	int n_strs = 0;
	int i;
	int len = 0;

	add_string(USER_START, USER_START_L);
	add_pstr(user_uri);
	add_string(USER_END, USER_END_L);
	add_string(USER_ETAG, USER_ETAG_L);

	if (_l < len) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "location_add_user(): Buffer too small\n");
		return -1;
	}

	for (i = 0; i < n_strs; i++)
		str_append(_b, strs[i].s, strs[i].len);

	return 0;
}

/*
 * Create start of location document
 */
int location_doc_start(str* _b, int _l)
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
	add_string(LOCATIONINFO_STAG, LOCATIONINFO_STAG_L);
	add_string(CRLF, CRLF_L);

	if (_l < len) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "location_add_resource(): Buffer too small\n");
		return -1;
	}

	for (i = 0; i < n_strs; i++)
		str_append(_b, strs[i].s, strs[i].len);

	return 0;
}

/*
 * Start a resource in a location document
 */
int location_doc_start_userlist(str* _b, int _l, str* _uri)
{
	str strs[10];
	int n_strs = 0;
	int i;
	int len = 0;

	add_string(USERLIST_START, USERLIST_START_L);
	add_string(CRLF, CRLF_L);

	if (_l < len) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "location_add_resource(): Buffer too small\n");
		return -1;
	}

	for (i = 0; i < n_strs; i++)
		str_append(_b, strs[i].s, strs[i].len);

	return 0;
}

/*
 * End a userlist in a location document
 */
int location_doc_end_resource(str *_b, int _l)
{
	str strs[10];
	int n_strs = 0;
	int i;
	int len = 0;

	add_string(USERLIST_ETAG, USERLIST_ETAG_L);
	add_string(CRLF, CRLF_L);

	if (_l < len) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "location_add_resource(): Buffer too small\n");
		return -1;
	}

	for (i = 0; i < n_strs; i++)
		str_append(_b, strs[i].s, strs[i].len);

	return 0;
}

/*
 * End a location document
 */
int location_doc_end(str* _b, int _l)
{
	if (_l < (LOCATIONINFO_ETAG_L + CRLF_L)) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "end_pidf_doc(): Buffer too small\n");
		return -1;
	}

	str_append(_b, LOCATIONINFO_ETAG CRLF, LOCATIONINFO_ETAG_L + CRLF_L);
	return 0;
}


int location_placeid_n_rows;

struct location_placeid_row {
     str room_name;
     int placeid;
} *location_placeid_table;

static int compare_location_placeid_rows(const void *v1, const void *v2)
{
     const struct location_placeid_row *r1 = v1;
     const struct location_placeid_row *r2 = v2;
     return str_strcasecmp(&r1->room_name, &r2->room_name);
}

static int places_initialized = 0;

int pa_location_init(void)
{
     if (use_db) {
	  db_key_t query_cols[2];
	  db_op_t  query_ops[2];
	  db_val_t query_vals[2];

	  db_key_t result_cols[4];
	  db_res_t *res;
	  int n_query_cols = 0;
	  int n_result_cols = 0;
	  int room_col, placeid_col;

	  result_cols[room_col = n_result_cols++] = "room";
	  result_cols[placeid_col = n_result_cols++] = "placeid";

	  if (pa_dbf.use_table(pa_db, place_table) < 0) {
		  LOG(L_ERR, "pa_location_init: Error in use_table\n");
		  return -1;
	  }

	  if (pa_dbf.query (pa_db, query_cols, query_ops, query_vals,
			result_cols, n_query_cols, n_result_cols, 0, &res) < 0) {
		  LOG(L_ERR, "pa_location_init: Error while querying tuple\n");
		  return -1;
	  }
	  LOG(L_ERR, "pa_location_init: res=%p res->n=%d\n", res, res->n);
	  if (res && res->n > 0) {
	       int r;
	       location_placeid_n_rows = res->n;
	       location_placeid_table = shm_malloc(res->n * sizeof(struct location_placeid_row));
	       for (r = 0; r < res->n; r++) {
		    db_row_t *res_row = &res->rows[r];
		    db_val_t *row_vals = ROW_VALUES(res_row);
		    str room_name; 
		    struct location_placeid_row *row = 
			 &location_placeid_table[r];
		    char *s;
		    room_name.s = row_vals[room_col].val.str_val.s;
		    row->room_name.len = room_name.len = strlen(room_name.s);
		    s = shm_malloc(row->room_name.len + 1);

		    row->placeid = row_vals[placeid_col].val.int_val;

		    row->room_name.s = s;
		    row->room_name.len = room_name.len;
		    strncpy(row->room_name.s, room_name.s, room_name.len);
		    row->room_name.s[room_name.len] = 0;
		    
		    LOG(L_ERR, "  placeid=%04d (nul=%d) room=%s len=%d (nul=%d)\n", 
			row->placeid, row_vals[placeid_col].nul,
			row->room_name.s, row->room_name.len, 
			row_vals[room_col].nul);
	       }
	  }
	  pa_dbf.free_query(pa_db, res);
	  if (use_bsearch)
	       qsort(location_placeid_table, location_placeid_n_rows,
		     sizeof(struct location_placeid_row),
		     compare_location_placeid_rows);
     }
     return 0;
}

int location_lookup_placeid(str *room_name, int *placeid)
{
     int i;
     LOG(L_ERR, "location_lookup_placeid: room_name=%.*s\n", room_name->len, room_name->s);
     if (!places_initialized) {
       pa_location_init();
       places_initialized = 1;
     }

     if (!use_bsearch) {
	  for (i = 0; i < location_placeid_n_rows; i++) {
	       struct location_placeid_row *row = &location_placeid_table[i];
	       if (str_strcasecmp(room_name, &row->room_name) == 0) {
		    *placeid = row->placeid;
		    LOG(L_ERR, "  placeid=%d\n", row->placeid);
		    return 1;
	       }
	  }
	  *placeid = 0;
	  return 0;
     } else {
	  struct location_placeid_row *row = 
	       bsearch(room_name,
		       location_placeid_table, location_placeid_n_rows,
		       sizeof(struct location_placeid_row),
		       compare_location_placeid_rows);
	  if (row) {
	       *placeid = row->placeid;
	       LOG(L_ERR, "  placeid=%d (bsearch)\n", row->placeid);
	       return 1;
	  } else {
	       *placeid = 0;
	       return 0;
	  }
     }

}
