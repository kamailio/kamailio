/*
 * $Id$
 *
 * List of registered domains
 */

#ifndef DLIST_H
#define DLIST_H

#include "udomain.h"
#include "../../str.h"


/*
 * List of all domains registered with usrloc
 */
typedef struct dlist {
	str name;            /* Name of the domain */
	udomain_t* d;        /* Payload */
	struct dlist* next;  /* Next element in the list */
} dlist_t;


extern dlist_t* root;

/*
 * Function registers a new domain with usrloc
 * if the domain exists, pointer to existing structure
 * will be returned, otherwise a new domain will be
 * created
 */
int register_udomain(const char* _n, udomain_t** _d);


/*
 * Free all registered domains
 */
void free_all_udomains(void);


/*
 * Just for debugging
 */
void print_all_udomains(void);


/*
 * Called from timer
 */
int timer_handler(void);


/*
 * Preload content of all domains from database
 */
int preload_all_udomains(void);


#endif /* UDLIST_H */
