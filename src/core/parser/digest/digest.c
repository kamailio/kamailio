/*
 * Digest credentials parser interface
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "digest.h"
#include "../../mem/mem.h"  /* pkg_malloc */
#include "../../dprint.h"   /* Guess what */
#include <stdio.h>          /* printf */
#include <string.h>         /* strncasecmp */


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
	void** ph_parsed;

	if (_h->parsed) {
		return 0;  /* Already parsed */
	}

	if (new_credentials(_h) < 0) {
		LOG(L_ERR, "parse_credentials(): Can't create new credentials\n");
		return -1;
	}

	     /* parse_digest_cred must return < -1 on error otherwise we will be
	      * unable to distinguish if the error was caused by the server or if the
	      * credentials are broken
	      */
	res = parse_digest_cred(&(_h->body), &(((auth_body_t*)(_h->parsed))->digest));
	
	if (res != 0) {
		ph_parsed=&_h->parsed;
		free_credentials((auth_body_t**)ph_parsed);
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
	if (_c->username.user.s == 0) res |= E_DIG_USERNAME;

	     /* Realm must be present */
	if (_c->realm.s == 0)  res |= E_DIG_REALM;

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
	case HDR_AUTHORIZATION_T: f = _m->authorization; break;
	case HDR_PROXYAUTH_T:     f = _m->proxy_auth;    break;
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


/*
 * Find credentials with given realm in a SIP message header
 */
int find_credentials(struct sip_msg* msg, str* realm,
		     hdr_types_t hftype, struct hdr_field** hdr)
{
	struct hdr_field** hook, *ptr;
	hdr_flags_t hdr_flags;
	int res;
	str* r;

	     /*
	      * Determine if we should use WWW-Authorization or
	      * Proxy-Authorization header fields, this parameter
	      * is set in www_authorize and proxy_authorize
	      */
	switch(hftype) {
	case HDR_AUTHORIZATION_T: 
		hook = &(msg->authorization);
		hdr_flags=HDR_AUTHORIZATION_F;
		break;
	case HDR_PROXYAUTH_T:
		hook = &(msg->proxy_auth);
		hdr_flags=HDR_PROXYAUTH_F;
		break;
	default:				
		hook = &(msg->authorization);
		hdr_flags=HDR_T2F(hftype);
		break;
	}

	     /*
	      * If the credentials haven't been parsed yet, do it now
	      */
	if (*hook == 0) {
		     /* No credentials parsed yet */
		if (parse_headers(msg, hdr_flags, 0) == -1) {
			LOG(L_ERR, "auth:find_credentials: Error while parsing headers\n");
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
			LOG(L_ERR, "auth:find_credentials: Error while parsing credentials\n");
			return (res == -1) ? -2 : -3;
		} else if (res == 0) {
			r = &(((auth_body_t*)(ptr->parsed))->digest.realm);

			if (r->len == realm->len) {
				if (!strncasecmp(realm->s, r->s, r->len)) {
					*hdr = ptr;
					return 0;
				}
			}
		}

		if (parse_headers(msg, hdr_flags, 1) == -1) {
			LOG(L_ERR, "auth:find_credentials: Error while parsing headers\n");
			return -4;
		} else {
			ptr = next_sibling_hdr(ptr);
			if (!ptr)
				break;
		}
	}
	
	     /*
	      * Credentials with given realm not found
	      */
	return 1;
}
