/*
 * $Id$
 *
 * Digest credentials parser interface
 */

#ifndef DIGEST_H
#define DIGEST_H

#include "digest_parser.h"
#include "../hf.h"         /* struct hdr_field */
#include "../msg_parser.h"


typedef struct auth_body {
	     /* This is pointer to header field containing
	      * parsed authorized digest credentials. This
	      * pointer is set in sip_msg->{authorization,proxy_auth}
	      * hooks.
	      *
	      * This is necessary for functions called after
	      * {www,proxy}_authorize, these functions need to know
	      * which credentials are authorized and they will simply
	      * look into 
	      * sip_msg->{authorization,proxy_auth}->parsed->authorized
	      */
	struct hdr_field* authorized;
	dig_cred_t digest;           /* Parsed digest credentials */
	unsigned char stale;         /* Flag is set if nonce is stale */
	int nonce_retries;           /* How many times the nonce was used */
} auth_body_t;


/*
 * Errors returned by check_dig_cred
 */
typedef enum dig_err {
	E_DIG_OK = 0,        /* Everything is OK */
	E_DIG_USERNAME  = 1, /* Username missing */
	E_DIG_REALM = 2,     /* Realm missing */
	E_DIG_NONCE = 4,     /* Nonce value missing */
	E_DIG_URI = 8,       /* URI missing */
	E_DIG_RESPONSE = 16, /* Response missing */
	E_DIG_CNONCE = 32,   /* CNONCE missing */
	E_DIG_NC = 64,       /* Nonce-count missing */
} dig_err_t;


/*
 * Parse digest credentials
 */
int parse_credentials(struct hdr_field* _h);


/*
 * Free all memory associated with parsed
 * structures
 */
void free_credentials(auth_body_t** _b);


/*
 * Print dig_cred structure to stdout
 */
void print_cred(dig_cred_t* _c);


/*
 * Mark credentials as authorized
 */
int mark_authorized_cred(struct sip_msg* _m, struct hdr_field* _h);


/*
 * Get pointer to authorized credentials
 */
int get_authorized_cred(struct hdr_field* _f, struct hdr_field** _h);


/*
 * Check if credentials are correct
 * (check of semantics)
 */
dig_err_t check_dig_cred(dig_cred_t* _c);


#endif /* DIGEST_H */
