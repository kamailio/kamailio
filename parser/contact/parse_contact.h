/*
 * $Id$
 *
 * Contact header field body parser
 */

#ifndef PARSE_CONTACT_H
#define PARSE_CONTACT_H

#include "../hf.h"
#include "../../str.h"
#include "contact.h"


typedef struct contact_body {
	unsigned char star;    /* Star contact */
	contact_t* contacts;   /* List of contacts */
} contact_body_t;


/*
 * Parse contact header field body
 */
int parse_contact(struct hdr_field* _h);


/*
 * Free all memory
 */
void free_contact(contact_body_t** _c);


/*
 * Print structure, for debugging only
 */
void print_contact(contact_body_t* _c);


#endif /* PARSE_CONTACT_H */
