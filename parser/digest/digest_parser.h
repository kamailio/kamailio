/*
 * $Id$
 *
 * Digest credentials parser
 */


#ifndef DIGEST_PARSER_H
#define DIGEST_PARSER_H

#include "../../str.h"


/* Type of algorithm used */
typedef enum alg {
	ALG_UNSPEC = 0,   /* Algorithm parameter not specified */
	ALG_MD5 = 1,      /* MD5 - default value*/
	ALG_MD5SESS = 2,  /* MD5-Session */
	ALG_OTHER = 4     /* Unknown */
} alg_t;


/* Quality Of Protection used */
typedef enum qop_type { 
	QOP_UNSPEC = 0,   /* QOP parameter not present in response */
	QOP_AUTH = 1,     /* Authentication only */
	QOP_AUTHINT = 2,  /* Authentication with integrity checks */
	QOP_OTHER = 4     /* Unknown */
} qop_type_t;


/* Algorithm structure */
struct algorithm {
	str alg_str;       /* The original string representation */
	alg_t alg_parsed;  /* Parsed value */
};


/* QOP structure */
struct qp {
	str qop_str;           /* The original string representation */
	qop_type_t qop_parsed; /* Parsed value */
};


/*
 * Parsed digest credentials
 */
typedef struct dig_cred {
	str username;         /* Username */
	str realm;            /* Realm */
	str nonce;            /* Nonce value */
	str uri;              /* URI */
	str response;         /* Response string */
	str algorithm;        /* Algorithm in string representation */
	struct algorithm alg; /* Type of algorithm used */
	str cnonce;           /* Cnonce value */
	str opaque;           /* Opaque data string */
	struct qp qop;        /* Quality Of Protection */
	str nc;               /* Nonce count parameter */
} dig_cred_t;


/*
 * Initialize digest parser
 */
void init_digest_parser(void);


/*
 * Initialize a digest credentials structure
 */
void init_dig_cred(dig_cred_t* _c);


/*
 * We support Digest authentication only
 *
 * Returns:
 *  0 - if everything is OK
 * -1 - Error while parsing
 *  1 - Unknown scheme
 */
int parse_digest_cred(str* _s, dig_cred_t* _c);


#endif /* DIGEST_PARSER_H */
