/*
 * $Id$
 *
 * Contact datatype
 */

#ifndef CONTACT_H
#define CONTACT_H


#include "cparam.h"    /* cparam_t */
#include "../../str.h"


/*
 * Structure representing a Contac HF body
 */
typedef struct contact {
	str uri;                /* contact uri */
	cparam_t* q;            /* q parameter hook */
	cparam_t* expires;      /* expires parameter hook */
	cparam_t* method;       /* method parameter hook */
	cparam_t* params;       /* List of all parameters */
        struct contact* next; /* Next contact in the list */
} contact_t;


/*
 * Parse contacts in a Contact HF
 */
int parse_contacts(str* _s, contact_t** _c);


/*
 * Free list of contacts
 * _c is head of the list
 */
void free_contacts(contact_t** _c);


/*
 * Print list of contacts, just for debugging
 */
void print_contacts(contact_t* _c);


#endif /* CONTACT_H */
