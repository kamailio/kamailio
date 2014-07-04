/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 */


#ifndef RFC2617_H
#define RFC2617_H

#include "../../str.h"


#define HASHLEN 16
typedef char HASH[HASHLEN];


#define HASHHEXLEN 32
typedef char HASHHEX[HASHHEXLEN+1];


/*
 * Type of algorithm used
 */
typedef enum {
	HA_MD5,      /* Plain MD5 */
	HA_MD5_SESS, /* MD5-Session */
} ha_alg_t;


/*
 * Convert to hex form
 */
void cvt_hex(HASH Bin, HASHHEX Hex);


/* 
 * calculate H(A1) as per HTTP Digest spec 
 */
typedef void (*calc_HA1_t)(ha_alg_t _alg,      /* Type of algorithm */
	      str* _username,     /* username */
	      str* _realm,        /* realm */
	      str* _password,     /* password */
	      str* _nonce,        /* nonce string */
	      str* _cnonce,       /* cnonce */
	      HASHHEX _sess_key); /* Result will be stored here */
void calc_HA1(ha_alg_t _alg,      /* Type of algorithm */
	      str* _username,     /* username */
	      str* _realm,        /* realm */
	      str* _password,     /* password */
	      str* _nonce,        /* nonce string */
	      str* _cnonce,       /* cnonce */
	      HASHHEX _sess_key); /* Result will be stored here */

void calc_H(str *ent, HASHHEX hash);

/* calculate request-digest/response-digest as per HTTP Digest spec */
typedef void (*calc_response_t)(HASHHEX _ha1,       /* H(A1) */
		   str* _nonce,        /* nonce from server */
		   str* _nc,           /* 8 hex digits */
		   str* _cnonce,       /* client nonce */
		   str* _qop,          /* qop-value: "", "auth", "auth-int" */
		   int _auth_int,      /* 1 if auth-int is used */
		   str* _method,       /* method from the request */
		   str* _uri,          /* requested URL */
		   HASHHEX _hentity,   /* H(entity body) if qop="auth-int" */
		   HASHHEX _response); /* request-digest or response-digest */
void calc_response(HASHHEX _ha1,       /* H(A1) */
		   str* _nonce,        /* nonce from server */
		   str* _nc,           /* 8 hex digits */
		   str* _cnonce,       /* client nonce */
		   str* _qop,          /* qop-value: "", "auth", "auth-int" */
		   int _auth_int,      /* 1 if auth-int is used */
		   str* _method,       /* method from the request */
		   str* _uri,          /* requested URL */
		   HASHHEX _hentity,   /* H(entity body) if qop="auth-int" */
		   HASHHEX _response); /* request-digest or response-digest */


#endif /* RFC2617_H */
