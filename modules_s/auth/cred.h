/*
 * $Id$
 */

#ifndef CRED_H
#define CRED_H

/*
 * $Id$
 */

#include "../../str.h"
#include "../../parser/msg_parser.h"


/*
 * Structure cred represents parsed clients credentials
 */

#define SCHEME_DIGEST    1
#define SCHEME_UNKNOWN 256

#define ALG_MD5           1
#define ALG_MD5_SESS      2
#define ALG_UNKNOWN     256
#define ALG_UNSPECIFIED 512

#define QOP_AUTH        1
#define QOP_AUTH_INT    2
#define QOP_UNKNOWN   256
#define QOP_UNDEFINED 512


typedef struct cred {
	int scheme;
	str username;
	str realm;
	str nonce;
	str uri;
	str response;
	int algorithm;
	str cnonce;
	str opaque;
	int qop;
	str nonce_count;
} cred_t;


int init_cred(cred_t* _c);
int hf2cred(struct hdr_field* _hf, cred_t* _c);
int print_cred(cred_t* _c);
int validate_cred(cred_t* _c);

#endif
