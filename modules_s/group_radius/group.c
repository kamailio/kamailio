/*
 * $Id$
 *
 * Group membership checking over Radius
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
 * 2003-03-10 - created by janakj
 *
 */

#include <radiusclient.h>
#include "../../parser/hf.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../parse_to.h"
#include "../../parse_from.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_uri.h"
#include "group.h"


/*
 * Get actual Request-URI
 */
static inline int get_request_uri(struct sip_msg* _m, str* _u)
{
	     /* Use new_uri if present */
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
	     /* Double check that the header field is there
	      * and is parsed
	      */
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
	     /* Double check that the header field is there
	      * and is parsed
	      */
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
	db_key_t keys[3] = {user_column, group_column, domain_column};
	db_val_t vals[3];
	db_key_t col[1] = {group_column};
	db_res_t* res;
	str uri;
	int hf_type;
	struct sip_uri puri;
	struct hdr_field* h;
	struct auth_body* c;
	
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



/*
 * Check from Radius if a user belongs to a group. User-Name is digest
 * username or digest username@realm, SIP-Group is group, and Service-Type
 * is Group-Check.  SIP-Group is SER specific attribute and Group-Check is
 * SER specific service type value.
 */
int radius_is_user_in (struct sip_msg* _msg, char* _hf, char* _group)
{
	str *grp, user_name;
	dig_cred_t* cred;
	int hf_type;
	UINT4 service;
	VALUE_PAIR *send, *received;
	static char msg[4096];
	struct hdr_field* h;

	grp = (str*)_group; /* via fixup */
	send = received = 0;

	hf_type = (int)_hf;

	switch(hf_type) {
	case 1: /* Request-URI */
		if (get_request_uri(_msg, &uri) < 0) {
			LOG(L_ERR, "radius_is_user_in(): Error while extracting Request-URI\n");
			return -1;
		}
		break;

	case 2: /* To */
		if (get_to_uri(_msg, &uri) < 0) {
			LOG(L_ERR, "radius_is_user_in(): Error while extracting To\n");
			return -2;
		}
		break;

	case 3: /* From */
		if (get_from_uri(_msg, &uri) < 0) {
			LOG(L_ERR, "radius_is_user_in(): Error while extracting From\n");
			return -3;
		}
		break;

	case 4: /* Credentials */
		get_authorized_cred(_m->authorization, &h);
		if (!h) {
			get_authorized_cred(_m->proxy_auth, &h);
			if (!h) {
				LOG(L_ERR, "get_cred_user(): No authorized credentials found (error in scripts)\n");
				return -4;
			}
		}
		cred = (auth_body_t*)(h->parsed);
		break;
	}

	if (hf_type != 4) {
		if (parse_uri(uri.s, uri.len, &puri) < 0) {
			LOG(L_ERR, "is_user_in(): Error while parsing URI\n");
			return -5;
		}
	} else {
		if (cred->


	if (q_memchr(cred->username.s, '@', cred->username.len)) {
		if (rc_avpair_add(&send, PW_USER_NAME, cred->username.s, cred->username.len) == NULL) {
			LOG(L_ERR, "radius_is_user_in_group(): Error adding PW_USER_NAME\n");
			rc_avpair_free(send);
			return -1;
		}
	} else {
		user_name.len = cred->username.len + cred->realm.len + 1;
		user_name.s = malloc(user_name.len);
		if (!user_name.s) {
			LOG(L_ERR, "radius_is_user_in_group(): Memory allocation failure\n");
			return -1;
		}
		strncpy(user_name.s, cred->username.s, cred->username.len);
		user_name.s[cred->username.len] = '@';
		strncpy(user_name.s + cred->username.len + 1, cred->realm.s, cred->realm.len);
		if (rc_avpair_add(&send, PW_USER_NAME, user_name.s, user_name.len) == NULL) {
			free(user_name.s);
			LOG(L_ERR, "radius_is_user_in_group(): Error adding PW_USER_NAME\n");
			rc_avpair_free(send);
			return -1;
		}
		free(user_name.s);
	}

	if (rc_avpair_add(&send, PW_SIP_GROUP, grp->s, grp->len) == NULL) {
		LOG(L_ERR, "radius_is_user_in(): Error adding PW_SIP_GROUP\n");
	 	return -1;  	
	}

	service = PW_GROUP_CHECK;
	if (rc_avpair_add(&send, PW_SERVICE_TYPE, &service, 0) == NULL) {
		LOG(L_ERR, "radius_is_user_in(): Error adding PW_SERVICE_TYPE\n");
		rc_avpair_free(send);
	 	return -1;  	
	}

	if (rc_auth(0, send, &received, msg) == OK_RC) {
		DBG("radius_is_user_in(): Success\n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		return 1;
	} else {
		DBG("radius_is_user_in(): Failure\n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		return -1;
	}
}
