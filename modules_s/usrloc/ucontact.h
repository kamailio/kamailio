/* 
 * $Id$ 
 *
 * Usrloc contact structure
 */

#ifndef UCONTACT_H
#define UCONTACT_H


#include <time.h>
#include "../../str.h"


typedef enum cstate {
	CS_NEW,        /* New contact - not flushed yet */
	CS_SYNC,       /* Synchronized contact with the database */
	CS_DIRTY       /* Update contact - not flushed yet */
} cstate_t;


typedef struct ucontact {
	str* domain;           /* Pointer to domain name */
	str* aor;              /* Pointer to the address of record string in record structure*/
	str c;                 /* Contact address */
	time_t expires;        /* expires parameter */
	float q;               /* q parameter */
	str callid;            /* Call-ID header field */
        int cseq;              /* CSeq value */
	cstate_t state;        /* State of the contact */
	struct ucontact* next; /* Next contact in the linked list */
	struct ucontact* prev; /* Previous contact in the linked list */
} ucontact_t;


/*
 * Create a new contact structure
 */
int new_ucontact(str* _dom, str* _aor, str* _contact, time_t _e, float _q, 
		 str* _callid, int _cseq, ucontact_t** _c);


/*
 * Free all memory associated with given contact structure
 */
void free_ucontact(ucontact_t* _c);


/*
 * Print contact, for debugging purposes only
 */
void print_ucontact(ucontact_t* _c);


/*
 * Update existing contact in memory with new values
 */
int mem_update_ucontact(ucontact_t* _c, time_t _e, float _q, str* _cid, int _cs);



/* ===== State transition functions - for write back cache scheme ======== */


/*
 * Update state of the contact if we
 * are using write-back scheme
 */
void st_update_ucontact(ucontact_t* _c);


/*
 * Update state of the contact if we
 * are using write-back scheme
 * Returns 1 if the contact should be
 * deleted from memory immediatelly,
 * 0 otherwise
 */
int st_delete_ucontact(ucontact_t* _c);


/*
 * Called when the timer is about to delete
 * an expired contact, this routine returns
 * 1 if the contact should be removed from
 * the database and 0 otherwise
 */
int st_expired_ucontact(ucontact_t* _c);


/*
 * Called when the timer is about flushing the contact,
 * updates contact state and returns 1 if the contact
 * should be inserted, 2 if updated and 0 otherwise
 */
int st_flush_ucontact(ucontact_t* _c);


/* ==== Database related functions ====== */


/*
 * Insert contact into the database
 */
int db_insert_ucontact(ucontact_t* _c);


/*
 * Update contact in the database
 */
int db_update_ucontact(ucontact_t* _c);


/*
 * Delete contact from the database
 */
int db_delete_ucontact(ucontact_t* _c);


/* ====== Module interface ====== */


/*
 * Update ucontact with new values
 */
int update_ucontact(ucontact_t* _c, time_t _e, float _q, str* _cid, int _cs);


#endif /* UCONTACT_H */
