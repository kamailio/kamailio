/*
 * $Id$
 *
 * Group membership
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
 *
 * History:
 * --------
 * 2003-02-25 - created by janakj
 *
 */


#include <string.h>
#include "../../dprint.h"               /* Logging */
#include "../../db/db.h"                /* Generic database API */
#include "../../parser/digest/digest.h" /* get_authorized_cred */
#include "../../parser/hf.h"            /* Header Field types */
#include "../../parser/parse_from.h"    /* From parser */
#include "../../parser/parse_uri.h"
#include "group.h"
#include "group_mod.h"                   /* Module parameters */


/*
 * Get Request-URI
 */
static inline int get_request_uri(struct sip_msg* _m, str* _u)
{
	if (_m->new_uri.s) {
		_u->s = _m->new_uri.s;
		_u->len = _m->new_uri.len;
	} else {
		_u->s = _m->first_line.u.request.uri.s;
		_u->len = _m->first_line.u.request.uri.len;
	}

	return 0;
}


/*
 * Get To header field URI
 */
static inline int get_to_uri(struct sip_msg* _m, str* _u)
{
	if (!_m->to && ((parse_headers(_m, HDR_TO, 0) == -1) || (!_m->to))) {
		LOG(L_ERR, "get_to_uri(): Can't get To header field\n");
		return -1;
	}
	
	_u->s = ((struct to_body*)_m->to->parsed)->uri.s;
	_u->len = ((struct to_body*)_m->to->parsed)->uri.len;

	return 0;
}


/*
 * Get From header field URI
 */
static inline int get_from_uri(struct sip_msg* _m, str* _u)
{
	if (parse_from_header(_m) < 0) {
		LOG(L_ERR, "get_from_uri(): Error while parsing From body\n");
		return -1;
	}
	
	_u->s = ((struct to_body*)_m->from->parsed)->uri.s;
	_u->len = ((struct to_body*)_m->from->parsed)->uri.len;

	return 0;
}


/*
 * Check if username in specified header field is in a table
 */
int is_user_in(struct sip_msg* _msg, char* _hf, char* _grp)
{
	db_key_t keys[3];
	db_val_t vals[3];
	db_key_t col[1];
	db_res_t* res;
	str uri;
	int hf_type;
	struct sip_uri puri;
	struct hdr_field* h;
	struct auth_body* c = 0; /* Makes gcc happy */

	keys[0]=user_column;
	keys[1]=group_column;
	keys[2]=domain_column;
	col[0]=group_column;
	
	hf_type = (int)_hf;

	switch(hf_type) {
	case 1: /* Request-URI */
		if (get_request_uri(_msg, &uri) < 0) {
			LOG(L_ERR, "is_user_in(): Error while obtaining username from Request-URI\n");
			return -1;
		}
		break;

	case 2: /* To */
		if (get_to_uri(_msg, &uri) < 0) {
			LOG(L_ERR, "is_user_in(): Error while extracting To username\n");
			return -2;
		}
		break;

	case 3: /* From */
		if (get_from_uri(_msg, &uri) < 0) {
			LOG(L_ERR, "is_user_in(): Error while extracting From username\n");
			return -3;
		}
		break;

	case 4: /* Credentials */
		get_authorized_cred(_msg->authorization, &h);
		if (!h) {
			get_authorized_cred(_msg->proxy_auth, &h);
			if (!h) {
				LOG(L_ERR, "is_user_in(): No authorized credentials found (error in scripts)\n");
				return -1;
			}
		}
	
		c = (auth_body_t*)(h->parsed);
		break;
	}

	if (hf_type != 4) {
		if (parse_uri(uri.s, uri.len, &puri) < 0) {
			LOG(L_ERR, "is_user_in(): Error while parsing URI\n");
			return -5;
		}

		VAL_STR(vals) = puri.user;
		VAL_STR(vals + 2) = puri.host;
	} else {
		VAL_STR(vals) = c->digest.username.user;
		VAL_STR(vals + 2) = (c->digest.username.domain.len) ? (c->digest.username.domain) : (c->digest.realm);
	}
	
	VAL_TYPE(vals) = VAL_TYPE(vals + 1) = VAL_TYPE(vals + 2) = DB_STR;
	VAL_NULL(vals) = VAL_NULL(vals + 1) = VAL_NULL(vals + 2) = 0;

	VAL_STR(vals + 1) = *((str*)_grp);
	
	db_use_table(db_handle, table);
	if (db_query(db_handle, keys, 0, vals, col, (use_domain) ? (3): (2), 1, 0, &res) < 0) {
		LOG(L_ERR, "is_user_in(): Error while querying database\n");
		return -5;
	}
	
	if (RES_ROW_N(res) == 0) {
		DBG("is_user_in(): User is not in group \'%.*s\'\n", 
		    ((str*)_grp)->len, ((str*)_grp)->s);
		db_free_query(db_handle, res);
		return -6;
	} else {
		DBG("is_user_in(): User is in group \'%.*s\'\n", 
		    ((str*)_grp)->len, ((str*)_grp)->s);
		db_free_query(db_handle, res);
		return 1;
	}
}
