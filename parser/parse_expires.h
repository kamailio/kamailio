/*
 * $Id$
 *
 * Expires header field body parser
 */

#ifndef PARSE_EXPIRES_H
#define PARSE_EXPIRES_H

#include "../str.h"
#include "hf.h"


typedef struct exp_body {
	str text;          /* Original text representation */
	int val;           /* Parsed value */
} exp_body_t;


/*
 * Parse expires header field body
 */
int parse_expires(struct hdr_field* _h);


/*
 * Free all memory associated with exp_body_t
 */
void free_expires(exp_body_t** _e);


/*
 * Print exp_body_t content, for debugging only
 */
void print_expires(exp_body_t* _e);


#endif /* PARSE_EXPIRES_H */
