/* 
 * $Id$ 
 *
 * Usrloc record structure
 */

#ifndef URECORD_H
#define URECORD_H


#include <time.h>
#include "hslot.h"
#include "../../str.h"
#include "ucontact.h"


struct hslot;


/*
 * Basic hash table element
 */
typedef struct urecord {
	str* domain;                   /* Pointer to domain we belong to */
	str aor;                       /* Address of record */
	ucontact_t* contacts;          /* One or more contact fields */
	
	struct hslot* slot;            /* Collision slot in the hash table array we belong to */
	struct {
		struct urecord* prev;  /* Next item in the linked list */
		struct urecord* next;  /* Previous item in the linked list */
	} d_ll;
	struct {                         /* Linked list of all elements in hash table */
		struct urecord* prev;  /* Previous item in the list */
		struct urecord* next;  /* Next item in the list */
	} s_ll;
} urecord_t;


/* Create a new record */
int new_urecord(str* _dom, str* _aor, urecord_t** _r);


/* Free all memory associated with the element */
void free_urecord(urecord_t* _r);


/*
 * Print an element, for debugging purposes only
 */
void print_urecord(urecord_t* _r);


/*
 * Add a new contact
 */
int mem_insert_ucontact(urecord_t* _r, str* _c, time_t _e, float _q, 
			str* _cid, int _cs, struct ucontact** _con);


/*
 * Remove contact from the list
 */
void mem_delete_ucontact(urecord_t* _r, ucontact_t* _c);


/*
 * Timer handler
 */
int timer_urecord(urecord_t* _r);


/*
 * Delete the whole record from database
 */
int db_delete_urecord(urecord_t* _r);


/* ===== Module interface ======== */


/*
 * Release urecord previously obtained
 * through get_urecord
 */
void release_urecord(urecord_t* _r);


/*
 * Create and insert new contact
 * into urecord
 */
int insert_ucontact(urecord_t* _r, str* _c, time_t _e, float _q, str* _cid, int _cs, struct ucontact** _con);


/*
 * Delete ucontact from urecord
 */
int delete_ucontact(urecord_t* _r, struct ucontact* _c);


/*
 * Get pointer to ucontact with given contact
 */
int get_ucontact(urecord_t* _r, str* _c, struct ucontact** _co);


#endif /* URECORD_H */
