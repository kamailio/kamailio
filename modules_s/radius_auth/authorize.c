/*
 * $Id$
 *
 * Authorize related functions
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


#include "authorize.h"
#include "../../parser/hf.h"            /* HDR_PROXYAUTH & HDR_AUTHORIZATION */
#include "defs.h"                       /* ACK_CANCEL_HACK */
#include "../../str.h"
#include <string.h>                     /* memcmp */
#include "nonce.h"
#include "../../parser/digest/digest.h" /* dig_cred_t */
#include "common.h"                     /* send_resp */
#include "auth_mod.h"
#include "../../db/db.h"
#include "../../mem/mem.h"
#include "rfc2617.h"
#include "digest.h"

#define MESSAGE_400 "Bad Request"


static inline int get_ha1(str* _user, str* _realm, char* _table, char* _ha1)
{
	db_key_t keys[] = {user_column, realm_column};
	db_val_t vals[2];
	db_key_t col[] = {pass_column};
	db_res_t* res;

	str result;

#ifdef USER_DOMAIN_HACK
	char* at;
#endif

	VAL_TYPE(vals) = VAL_TYPE(vals + 1) = DB_STR;
	VAL_NULL(vals) = VAL_NULL(vals + 1) = 0;
	
	VAL_STR(vals).s = _user->s;
	VAL_STR(vals).len = _user->len;
	
	VAL_STR(vals + 1).s = _realm->s;
	VAL_STR(vals + 1).len = _realm->len;

#ifdef USER_DOMAIN_HACK
	at = memchr(_user->s, '@', _user->len);
	if (at) {
		DBG("get_ha1(): @ found in username, removing domain part\n");
		VAL_STR(vals).len = at - _user->s;
		if (!calc_ha1) {
			col[0] = pass_column_2;
		}
	}
#endif

	db_use_table(db_handle, _table);
	if (db_query(db_handle, keys, 0, vals, col, 2, 1, NULL, &res) < 0) {
		LOG(L_ERR, "get_ha1(): Error while querying database\n");
		return -1;
	}

	if (RES_ROW_N(res) == 0) {
		DBG("get_ha1(): no result\n");
		db_free_query(db_handle, res);
		return -1;
	}

        result.s = (char*)ROW_VALUES(RES_ROWS(res))[0].val.string_val;
	result.len = strlen(result.s);

	if (calc_ha1) {
		     /* Only plaintext passwords are stored in database,
		      * we have to calculate HA1 */
		calc_HA1(HA_MD5, _user, _realm, &result, NULL, NULL, _ha1);
		DBG("HA1 string calculated: %s\n", _ha1);
	} else {
		memcpy(_ha1, result.s, result.len);
		_ha1[result.len] = '\0';
	}

	db_free_query(db_handle, res);
	return 0;
}


/*
 * This is version of check_response is a modified version of what can be found in
 * auth module. It essentially just calls the function to check with the RADIUS server
 * and precipitates the response.
 */
static inline int check_response(dig_cred_t* _cred, str* _method, char* _ha1)
{
	if (_cred->response.len != 32) {
		LOG(L_ERR, "check_response(): Receive response len != 32\n");
		return -1;
	}

	/*
	 * Access the Radius Database...
	 */
	if (radius_authorize_freeradius(_cred, _method) == 0) {
		DBG("check_cred(): Authorization is OK\n");
		return 1;
	} else {
		DBG("check_cred(): Authorization failed\n");
		return -1;
	}

}


static inline int find_credentials(struct sip_msg* _m, str* _realm, int _hftype, struct hdr_field** _h)
{
	struct hdr_field** hook;
	struct hdr_field* ptr, *prev;
	int res;
	str* r;

	switch(_hftype) {
	case HDR_AUTHORIZATION: hook = &(_m->authorization); break;
	case HDR_PROXYAUTH:     hook = &(_m->proxy_auth);    break;
	default:
		LOG(L_ERR, "find_credentials(): Invalid header field typ as parameter\n");
		return -1;
	}

	*_h = 0;
	
	if (!(*hook)) {
		     /* No credentials parsed yet */
		if (parse_headers(_m, _hftype, 0) == -1) {
			LOG(L_ERR, "find_credentials(): Error while parsing headers\n");
			return -2;
		}
	}

	ptr = *hook;

	while(ptr) {
		res = parse_credentials(ptr);
		if (res < 0) {
			LOG(L_ERR, "find_credentials(): Error while parsing credentials\n");
			if (send_resp(_m, 400, MESSAGE_400, 0, 0) == -1) {
				LOG(L_ERR, "authorize(): Error while sending 400 reply\n");
			}
			return -1;
		} else if (res == 0) {
			r = &(((auth_body_t*)(ptr->parsed))->digest.realm);

			if (r->len == _realm->len) {
				if (!strncasecmp(_realm->s, r->s, r->len)) {
					*_h = ptr;
					return 0;
				}
			}
			
		}

		prev = ptr;
		if (parse_headers(_m, _hftype, 1) == -1) {
			LOG(L_ERR, "find_credentials(): Error while parsing headers\n");
			return -3;
		} else {
			if (prev != _m->last_header) {
				if (_m->last_header->type == _hftype) ptr = _m->last_header;
				else ptr = 0;
			} else ptr = 0;
		}
	}
	return 0;
}


/*
 * Authorize digest credentials
 */
static inline int authorize(struct sip_msg* _msg, str* _realm, char* _table, int _hftype)
{
	char ha1[256];
	int res;
	struct hdr_field* h;
	auth_body_t* cred;

#ifdef ACK_CANCEL_HACK
	     /* ACK must be always authorized, there is
	      * no way how to challenge ACK
	      */
	if (_msg->REQ_METHOD == METHOD_ACK) {
	        return 1;
	}
#endif

	     /* Try to find credentials with corresponding realm
	      * in the message, parse them and return pointer to
	      * parsed structure
	      */
	if (find_credentials(_msg, _realm, _hftype, &h) < 0) {
		LOG(L_ERR, "authorize(): Error while looking for credentials\n");
		return -1;
	}

	     /*
	      * No credentials with given realm found, dont' authorize
	      */
	if (h == 0) {
		DBG("authorize(): Credentials with given realm not found\n");
		return -1;
	}

	cred = (auth_body_t*)(h->parsed);

	     /* Check credentials correctness here 
	      * FIXME: 400s should be sent from routing scripts, but we will need
	      * variables for that
	      */
	if (check_dig_cred(&(cred->digest)) != E_DIG_OK) {
		LOG(L_ERR, "authorize(): Credentials received are not filled properly\n");

		if (send_resp(_msg, 400, MESSAGE_400, NULL, 0) == -1) {
			LOG(L_ERR, "authorize(): Error while sending 400 reply\n");
		}
		return 0;
	}

	if (check_nonce(&(cred->digest.nonce), &secret) == 0) {
		LOG(L_ALERT, "authorize(): Invalid nonce value received, very suspicious !\n");
		return -1;
	}

	     /* Retrieve number of retries with the received nonce and
	      * save it
	      */
	cred->nonce_retries = get_nonce_retry(&(cred->digest.nonce));

	     /* Stelios: Skipping the DB access...
		  * Calculate or fetch from the dabase HA1 string, which
	      * is necessary for request recalculation
	      */
        
	/* Recalculate response, it must be same to authorize sucessfully */
    res = check_response(&(cred->digest), &_msg->first_line.u.request.method, ha1);

	if (res == 1) {  /* response was OK */
		if (nonce_is_stale(&(cred->digest.nonce))) {
			if ((_msg->REQ_METHOD == METHOD_ACK) || 
			    (_msg->REQ_METHOD == METHOD_CANCEL)) {
				     /* Method is ACK or CANCEL, we must accept stale
				      * nonces because there is no way how to challenge
				      * with new nonce (ACK and CANCEL have no responses
				      * associated)
				      */
				goto mark;
			} else {
				DBG("authorize(): Response is OK, but nonce is stale\n");
				cred->stale = 1;
				return -1;
			}
		} else {
			DBG("authorize(): Authorization OK\n");
			goto mark;
		}
	} else {
		DBG("authorize(): Recalculated response is different\n");
		return -1;
	}

 mark:
	if (mark_authorized_cred(_msg, h) < 0) {
		LOG(L_ERR, "authorize(): Error while marking parsed credentials\n");
		return -1;
	}
	return 1;
}


/*
 * Authorize using Proxy-Authorize header field
 */
int radius_proxy_authorize(struct sip_msg* _msg, char* _realm, char* _table)
{
	     /* realm parameter is converted to str* in str_fixup */
	return authorize(_msg, (str*)_realm, _table, HDR_PROXYAUTH);
}


/*
 * Authorize using WWW-Authorize header field
 */
int radius_www_authorize(struct sip_msg* _msg, char* _realm, char* _table)
{
	return authorize(_msg, (str*)_realm, _table, HDR_AUTHORIZATION);
}


/*
 * Remove used credentials
 */
int consume_credentials(struct sip_msg* _m, char* _s1, char* _s2)
{
	struct hdr_field* h;

	get_authorized_cred(_m->authorization, &h);
	if (!h) {
		get_authorized_cred(_m->proxy_auth, &h);
		if (!h) {
			LOG(L_ERR, "consume_credentials(): No authorized credentials found (error in scripts)\n");
			return -1;
		}
	}
	
	if (del_lump(&_m->add_rm, h->name.s - _m->buf, h->name.len + h->body.len, 0) == 0) {
		LOG(L_ERR, "consume_credentials(): Can't remove credentials\n");
		return -1;
	}

	return 1;
}


