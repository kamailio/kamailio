/*
 * $Id$
 *
 * Digest response calculation as per RFC2617
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
void calc_HA1(ha_alg_t _alg,      /* Type of algorithm */
	      str* _username,     /* username */
	      str* _realm,        /* realm */
	      str* _password,     /* password */
	      str* _nonce,        /* nonce string */
	      str* _cnonce,       /* cnonce */
	      HASHHEX _sess_key); /* Result will be stored here */


/* calculate request-digest/response-digest as per HTTP Digest spec */
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
