/* 
 * $Id$ 
 *
 * Usrloc domain structure
 */

#ifndef UDOMAIN_H
#define UDOMAIN_H


#include "../../fastlock.h"
#include "../../str.h"
#include "urecord.h"
#include "hslot.h"


struct hslot;   /* Hash table slot */
struct urecord; /* Usrloc record */


/*
 * The structure represents a usrloc domain
 */
typedef struct udomain {
	str* name;                     /* Domain name */
	int size;                      /* Hash table size */
	struct hslot* table;           /* Hash table - array of collision slots */
	struct {                       /* Linked list of all elements in the domain */
		int n;                 /* Number of element in the linked list */
		struct urecord* first; /* First element in the list */
		struct urecord* last;  /* Last element in the list */
	} d_ll;
	fl_lock_t lock;                /* lock variable */
} udomain_t;


/*
 * Create a new domain structure
 * _n is pointer to str representing
 * name of the domain, the string is
 * not copied, it should point to str
 * structure stored in domain list
 * _s is hash table size
 */
int new_udomain(str* _n, int _s, udomain_t** _d);


/*
 * Free all memory allocated for
 * the domain
 */
void free_udomain(udomain_t* _d);


/*
 * Just for debugging
 */
void print_udomain(udomain_t* _d);


/*
 * Load data from a database
 */
int preload_udomain(udomain_t* _d);


/*
 * Timer handler for given domain
 */
int timer_udomain(udomain_t* _d);


/*
 * Insert record into domain
 */
int insert_urecord(udomain_t* _d, struct urecord* _r);


/*
 * Find record with specified AOR
 */
int get_urecord(udomain_t* _d, str* _a, struct urecord** _r);


/*
 * Delete a record
 */
int delete_urecord(udomain_t* _d, str* _a);


/*
 * Release a record structure previously obtained
 * using get_record
 */
void release_urecord(struct urecord* _r);



#endif /* UDOMAIN_H */
