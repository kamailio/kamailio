/*
 * $Id$
 *
 * 32-bit Digest parameter name parser
 */

#ifndef PARAM_PARSER_H
#define PARAM_PARSER_H

#include "../../str.h"


/*
 * Digest parameter types
 */
typedef enum dig_par {
	PAR_USERNAME,  /* username parameter */
	PAR_REALM,     /* realm parameter */
	PAR_NONCE,     /* nonce parameter */
	PAR_URI,       /* uri parameter */
	PAR_RESPONSE,  /* response parameter */
	PAR_CNONCE,    /* cnonce parameter */
	PAR_OPAQUE,    /* opaque parameter */
	PAR_QOP,       /* qop parameter */
	PAR_NC,        /* nonce-count parameter */
	PAR_ALGORITHM, /* algorithm parameter */
	PAR_OTHER      /* unknown parameter */
} dig_par_t;


/*
 * Parse digest parameter name
 */
int parse_param_name(str* _s, dig_par_t* _type);


/*
 * Initialize hash table
 */
void init_digest_htable(void);


#endif /* PARAM_PARSER_H */
