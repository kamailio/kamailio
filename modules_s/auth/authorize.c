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
 *
 * history:
 * ---------
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 */


#include <string.h>                     /* memcmp */
#include "../../comp_defs.h"
#include "../../parser/hf.h"            /* HDR_PROXYAUTH & HDR_AUTHORIZATION */
#include "../../str.h"
#include "../../parser/digest/digest.h" /* dig_cred_t */
#include "../../db/db.h"                /* Database API */
#include "../../mem/mem.h"              /* Memory subsystem */
#include "authorize.h"
#include "defs.h"                       /* ACK_CANCEL_HACK */
#include "../../parser/parse_uri.h"
#include "nonce.h"                      /* Nonce related functions */
#include "common.h"                     /* send_resp */
#include "auth_mod.h"
#include "rfc2617.h"


#define MESSAGE_500 "Server Internal Error"


/*
 * Get or calculate HA1 string, if calculate_ha1 is set, the function will
 * simply fetch the string from the database, otherwise it will fetch plaintext
 * password and will calculate the string
 */
static inline int get_ha1(str* _user, str* _realm, char* _table, char* _ha1)
{
	db_key_t keys[] = {user_column, domain_column};
	db_val_t vals[2];
	db_key_t col[] = {pass_column};
	db_res_t* res;

	str result;

#ifdef USER_DOMAIN_HACK
	char* at;
#endif

	VAL_TYPE(vals) = VAL_TYPE(vals + 1) = DB_STR;
	VAL_NULL(vals) = VAL_NULL(vals + 1) = 0;
	
	VAL_STR(vals) = *_user;
	VAL_STR(vals + 1) = *_realm;

	     /*
	      * Some user agents put domain in the username, since we
	      * have only usernames in database, remove domain part
	      * if the server uses HA1 precalculated strings in the
	      * database, then switch over to another column, which
	      * contains HA1 strings calculated also with domain, the
	      * original column contains HA1 strings calculated without
	      * the domain part
	      */
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

	     /* 
	      * Query the database either for HA1 string or plaintext password,
	      * it depends on calculate_ha1 variable value
	      */
	db_use_table(db_handle, _table);
	if (db_query(db_handle, keys, 0, vals, col, 2, 1, 0, &res) < 0) {
		LOG(L_ERR, "get_ha1(): Error while querying database\n");
		return -1;
	}

	     /*
	      * There is no such username in the database, return 1
	      */
	if (RES_ROW_N(res) == 0) {
		DBG("get_ha1(): no result for user \'%.*s\'\n", _user->len, _user->s);
		db_free_query(db_handle, res);
		return 1;
	}

        result.s = (char*)ROW_VALUES(RES_ROWS(res))[0].val.string_val;
	result.len = strlen(result.s);

	     /*
	      * If calculate_ha1 variable is set to true, calculate HA1 
	      * string on the fly from username, realm and plaintext 
	      * password obtained from the database and return the 
	      * calculated HA1 string
	      *
	      * If calculate_ha1 is not set, we have the HA1 already,
	      * just return it
	      */
	if (calc_ha1) {
		     /* Only plaintext passwords are stored in database,
		      * we have to calculate HA1 */
		calc_HA1(HA_MD5, _user, _realm, &result, 0, 0, _ha1);
		DBG("get_ha1(): HA1 string calculated: \'%s\'\n", _ha1);
	} else {
		memcpy(_ha1, result.s, result.len);
		_ha1[result.len] = '\0';
	}

	db_free_query(db_handle, res);
	return 0;
}


/*
 * Calculate the response and compare with the given response string
 * Authorization is successfull if this two strings are same
 */
static inline int check_response(dig_cred_t* _cred, str* _method, char* _ha1)
{
	HASHHEX resp, hent;

	     /*
	      * First, we have to verify that the response received has
	      * the same length as responses created by us
	      */
	if (_cred->response.len != 32) {
		DBG("check_response(): Receive response len != 32\n");
		return 1;
	}

	     /*
	      * Now, calculate our response from parameters received
	      * from the user agent
	      */
	calc_response(_ha1, &(_cred->nonce), 
		      &(_cred->nc), &(_cred->cnonce), 
		      &(_cred->qop.qop_str), _cred->qop.qop_parsed == QOP_AUTHINT,
		      _method, &(_cred->uri), hent, resp);
	
	DBG("check_response(): Our result = \'%s\'\n", resp);
	
	     /*
	      * And simply compare the strings, the user is
	      * authorized if they match
	      */
	if (!memcmp(resp, _cred->response.s, 32)) {
		DBG("check_response(): Authorization is OK\n");
		return 0;
	} else {
		DBG("check_response(): Authorization failed\n");
		return 2;
	}
}


/*
 * Find credentials with given realm in a SIP message header
 */
static inline int find_credentials(struct sip_msg* _m, str* _realm, int _hftype, struct hdr_field** _h)
{
	struct hdr_field** hook, *ptr, *prev;
	int res;
	str* r;

	     /*
	      * Determine if we should use WWW-Authorization or
	      * Proxy-Authorization header fields, this parameter
	      * is set in www_authorize and proxy_authorize
	      */
	switch(_hftype) {
	case HDR_AUTHORIZATION: hook = &(_m->authorization); break;
	case HDR_PROXYAUTH:     hook = &(_m->proxy_auth);    break;
	default:                hook = &(_m->authorization); break;
	}

	     /*
	      * If the credentials haven't been parsed yet, do it now
	      */
	if (*hook == 0) {
		     /* No credentials parsed yet */
		if (parse_headers(_m, _hftype, 0) == -1) {
			LOG(L_ERR, "find_credentials(): Error while parsing headers\n");
			return -1;
		}
	}

	ptr = *hook;

	     /*
	      * Iterate through the credentials in the message and
	      * find credentials with given realm
	      */
	while(ptr) {
		res = parse_credentials(ptr);
		if (res < 0) {
			LOG(L_ERR, "find_credentials(): Error while parsing credentials\n");
			return (res == -1) ? -2 : -3;
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
			return -4;
		} else {
			if (prev != _m->last_header) {
				if (_m->last_header->type == _hftype) ptr = _m->last_header;
				else break;
			} else break;
		}
	}
	
	     /*
	      * Credentials with given realm not found
	      */
	return 1;
}


/*
 * Authorize digest credentials
 */
static inline int authorize(struct sip_msg* _m, str* _realm, char* _table, int _hftype)
{
	char ha1[256];
	int res, ret;
	struct hdr_field* h;
	auth_body_t* cred;
	struct sip_uri uri;

	ret = -1;

#ifdef ACK_CANCEL_HACK
	     /* ACK and CANCEL must be always authorized, there is
	      * no way how to challenge ACK and CANCEL cannot be
	      * challenged because it must have the same CSeq as
	      * the request to be cancelled
	      */
	if ((_m->REQ_METHOD == METHOD_ACK) || 
	    (_m->REQ_METHOD == METHOD_CANCEL)) {
	        return 1;
	}
#endif

#ifdef AUTO_REALM
	if (_realm->len == 0) {
		if (get_realm(_m, &uri) < 0) {
			LOG(L_ERR, "authorize(): Error while extracting realm\n");
			if (send_resp(_m, 400, MESSAGE_400, 0, 0) == -1) {
				LOG(L_ERR, "authorize(): Error while sending 400 reply\n");
				return -1;
			}
			return 0;
		}
		
		_realm = &uri.host;
	}
#endif


	     /* Try to find credentials with corresponding realm
	      * in the message, parse them and return pointer to
	      * parsed structure
	      */
	res = find_credentials(_m, _realm, _hftype, &h);
	if (res < 0) {
		LOG(L_ERR, "authorize(): Error while looking for credentials\n");
		if (send_resp(_m, (res == -2) ? 500 : 400, 
			      (res == -2) ? MESSAGE_500 : MESSAGE_400, 0, 0) == -1) {
			LOG(L_ERR, "authorize(): Error while sending 400 reply\n");
			goto err;
		}
		ret = 0;
		goto err;
	} else if (res > 0) {
		DBG("authorize(): Credentials with given realm not found\n");
		goto err;
	}

	     /* Pointer to the parsed credentials */
	cred = (auth_body_t*)(h->parsed);

	     /* Check credentials correctness here */
	if (check_dig_cred(&(cred->digest)) != E_DIG_OK) {
		LOG(L_ERR, "authorize(): Credentials received are not filled properly\n");
		if (send_resp(_m, 400, MESSAGE_400, 0, 0) == -1) {
			LOG(L_ERR, "authorize(): Error while sending 400 reply\n");
			goto err;
		}
		ret = 0;
		goto err;
	}

	if (check_nonce(&(cred->digest.nonce), &secret) != 0) {
		LOG(L_ALERT, "authorize(): Invalid nonce value received, very suspicious !\n");
		goto err;
	}

	     /* Retrieve number of retries with the received nonce and
	      * save it
	      */
	cred->nonce_retries = get_nonce_retry(&(cred->digest.nonce));

	     /* Calculate or fetch from the dabase HA1 string, which
	      * is necessary for request recalculation
	      */
	res = get_ha1(&cred->digest.username, _realm, _table, ha1);
        if (res < 0) {
		     /* Error while accessing the database */
		if (send_resp(_m, 500, MESSAGE_500, 0, 0) == -1) {
			LOG(L_ERR, "authorize(): Error while sending 500 reply\n");
			goto err;
		}
		ret = 0;
		goto err;
	} else if (res > 0) {
		     /* Username not found */
		goto err;
	}

	     /* Recalculate response, it must be same to authorize sucessfully */
        res = check_response(&(cred->digest), &_m->first_line.u.request.method, ha1);

	if (res == 0) {  /* response was OK */
		if (!nonce_is_stale(&(cred->digest.nonce))) {
			if ((_m->REQ_METHOD == METHOD_ACK) || 
			    (_m->REQ_METHOD == METHOD_CANCEL)) {
				     /* Method is ACK or CANCEL, we must accept stale
				      * nonces because there is no way how to challenge
				      * with new nonce (ACK has no response associated 
				      * and CANCEL must have the same CSeq as the request 
				      * to be cancelled)
				      */
				goto mark;
			} else {
				DBG("authorize(): Response is OK, but nonce is stale\n");
				cred->stale = 1;
				goto err;
			}
		} else {
			DBG("authorize(): Authorization OK\n");
			goto mark;
		}
	} else {
		DBG("authorize(): Recalculated response is different\n");
		goto err;
	}

 mark:
	if (mark_authorized_cred(_m, h) < 0) {
		LOG(L_ERR, "authorize(): Error while marking parsed credentials\n");
		if (send_resp(_m, 500, MESSAGE_500, 0, 0) == -1) {
			LOG(L_ERR, "authorize(): Error while sending 500 reply\n");
			goto err;
		}
		ret = 0;
	} else {
		ret = 1;
	}

 err:
	return ret;
}


/*
 * Authorize using Proxy-Authorize header field
 */
int proxy_authorize(struct sip_msg* _m, char* _realm, char* _table)
{
	     /* realm parameter is converted to str* in str_fixup */
	return authorize(_m, (str*)_realm, _table, HDR_PROXYAUTH);
}


/*
 * Authorize using WWW-Authorize header field
 */
int www_authorize(struct sip_msg* _m, char* _realm, char* _table)
{
	return authorize(_m, (str*)_realm, _table, HDR_AUTHORIZATION);
}


/*
 * Remove used credentials from a SIP message header
 */
int consume_credentials(struct sip_msg* _m, char* _s1, char* _s2)
{
	struct hdr_field* h;
	int len;

	get_authorized_cred(_m->authorization, &h);
	if (!h) {
		get_authorized_cred(_m->proxy_auth, &h);
		if (!h) {
			LOG(L_ERR, "consume_credentials(): No authorized credentials found (error in scripts)\n");
			return -1;
		}
	}

#ifdef PRESERVE_ZT
	if (h->next) len = h->next->name.s - h->name.s;
	else len = _m->unparsed - h->name.s;
#else
	len=h->len;
#endif

	if (del_lump(&_m->add_rm, h->name.s - _m->buf, len, 0) == 0) {
		LOG(L_ERR, "consume_credentials(): Can't remove credentials\n");
		return -1;
	}

	return 1;
}
