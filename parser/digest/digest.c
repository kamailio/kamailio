/*
 * $Id$
 *
 * Digest credentials parser interface
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


#include "digest.h"
#include "../../mem/mem.h"  /* pkg_malloc */
#include "../../dprint.h"   /* Guess what */
#include <stdio.h>          /* printf */


/*
 * Create and initialize a new credentials structure
 */
static inline int new_credentials(struct hdr_field* _h)
{
	auth_body_t* b;

	b = (auth_body_t*)pkg_malloc(sizeof(auth_body_t));
	if (b == 0) {
		LOG(L_ERR, "parse_credentials(): No memory left\n");
		return -1;
	}
		
	init_dig_cred(&(b->digest));
	b->stale = 0;
	b->nonce_retries = 0;
	b->authorized = 0;

	_h->parsed = (void*)b;

	return 0;
}


/*
 * Parse digest credentials
 * Return value -1 means that the function was unable to allocate
 * memory and therefore the server should return Internal Server Error,
 * not Bad Request in this case !
 * Bad Request should be send when return value != -1
 */
int parse_credentials(struct hdr_field* _h)
{
	int res;

	if (_h->parsed) {
		return 0;  /* Already parsed */
	}

	if (new_credentials(_h) < 0) {
		LOG(L_ERR, "parse_credentials(): Can't create new credentials\n");
		return -1;
	}

	     /* parse_digest_cred must return < -1 on error otherwise we will be
	      * unable to distinguis if the error was caused by the server or if the
	      * credentials are broken
	      */
	res = parse_digest_cred(&(_h->body), &(((auth_body_t*)(_h->parsed))->digest));
	
	if (res != 0) {
		free_credentials((auth_body_t**)&(_h->parsed));
	}

	return res;
}


/*
 * Free all memory
 */
void free_credentials(auth_body_t** _b)
{
	pkg_free(*_b);
	*_b = 0;
}


/*
 * Check semantics of a digest credentials structure
 * Make sure that all attributes needed to verify response 
 * string are set or at least have a default value
 *
 * The returned value is logical OR of all errors encountered
 * during the check, see dig_err_t type for more details 
 */
dig_err_t check_dig_cred(dig_cred_t* _c)
{
	dig_err_t res = E_DIG_OK;

	     /* Username must be present */
	if (_c->username.whole.s == 0) res |= E_DIG_USERNAME;

	     /* Realm must be present */
	if (_c->realm.s == 0) res |= E_DIG_REALM;

	     /* Nonce that was used must be specified */
	if (_c->nonce.s == 0) res |= E_DIG_NONCE;

	     /* URI must be specified */
	if (_c->uri.s == 0) res |= E_DIG_URI;

	     /* We cannot check credentials without response */
	if (_c->response.s == 0) res |= E_DIG_RESPONSE;

	     /* If QOP parameter is present, some additional
	      * requirements must be met
	      */
	if ((_c->qop.qop_parsed == QOP_AUTH) || (_c->qop.qop_parsed == QOP_AUTHINT)) {
		     /* CNONCE must be specified */
		if (_c->cnonce.s == 0) res |= E_DIG_CNONCE;
		     /* and also nonce count must be specified */
		if (_c->nc.s == 0) res |= E_DIG_NC;
	}
		
	return res;	
}


/*
 * Print credential structure content to stdout
 * Just for debugging
 */
void print_cred(dig_cred_t* _c)
{
	printf("===Digest credentials===\n");
	if (_c) {
		printf("Username\n");
		printf("+--whole  = \'%.*s\'\n", _c->username.whole.len, _c->username.whole.s);
		printf("+--user   = \'%.*s\'\n", _c->username.user.len, _c->username.user.s);
		printf("\\--domain = \'%.*s\'\n", _c->username.domain.len, _c->username.domain.s);
		printf("Realm     = \'%.*s\'\n", _c->realm.len, _c->realm.s);
		printf("Nonce     = \'%.*s\'\n", _c->nonce.len, _c->nonce.s);
		printf("URI       = \'%.*s\'\n", _c->uri.len, _c->uri.s);
		printf("Response  = \'%.*s\'\n", _c->response.len, _c->response.s);
		printf("Algorithm = \'%.*s\'\n", _c->alg.alg_str.len, _c->alg.alg_str.s);
		printf("\\--parsed = ");

		switch(_c->alg.alg_parsed) {
		case ALG_UNSPEC:  printf("ALG_UNSPEC\n");  break;
		case ALG_MD5:     printf("ALG_MD5\n");     break;
		case ALG_MD5SESS: printf("ALG_MD5SESS\n"); break;
		case ALG_OTHER:   printf("ALG_OTHER\n");   break;
		}

		printf("Cnonce    = \'%.*s\'\n", _c->cnonce.len, _c->cnonce.s);
		printf("Opaque    = \'%.*s\'\n", _c->opaque.len, _c->opaque.s);
		printf("QOP       = \'%.*s\'\n", _c->qop.qop_str.len, _c->qop.qop_str.s);
		printf("\\--parsed = ");

		switch(_c->qop.qop_parsed) {
		case QOP_UNSPEC:  printf("QOP_UNSPEC\n");  break;
		case QOP_AUTH:    printf("QOP_AUTH\n");    break;
		case QOP_AUTHINT: printf("QOP_AUTHINT\n"); break;
		case QOP_OTHER:   printf("QOP_OTHER\n");   break;
		}
		printf("NC        = \'%.*s\'\n", _c->nc.len, _c->nc.s);
	}
	printf("===/Digest credentials===\n");
}


/*
 * Mark credentials as selected so functions
 * following authorize know which credentials
 * to use if the message contained more than
 * one
 */
int mark_authorized_cred(struct sip_msg* _m, struct hdr_field* _h)
{
	struct hdr_field* f;
	
	switch(_h->type) {
	case HDR_AUTHORIZATION: f = _m->authorization; break;
	case HDR_PROXYAUTH:     f = _m->proxy_auth;    break;
	default:
		LOG(L_ERR, "mark_authorized_cred(): Invalid header field type\n");
		return -1;
	}

	if (!(f->parsed)) {
		if (new_credentials(f) < 0) {
			LOG(L_ERR, "mark_authorized_cred(): Error in new_credentials\n");
			return -1;
		}
	}

	((auth_body_t*)(f->parsed))->authorized = _h;

	return 0;
}


/*
 * Get pointer to authorized credentials, if there are no
 * authorized credentials, 0 is returned
 */
int get_authorized_cred(struct hdr_field* _f, struct hdr_field** _h)
{
	if (_f && _f->parsed) {
		*_h = ((auth_body_t*)(_f->parsed))->authorized;
	} else {
		*_h = 0;
	}
	
	return 0;
}
