/* 
 * $Id$ 
 *
 * Usrloc contact structure
 */

#ifndef UCONTACT_H
#define UCONTACT_H


#include <time.h>
#include "../../str.h"


typedef struct ucontact {
	str* domain;           /* Pointer to domain name */
	str* aor;              /* Pointer to the address of record string in record structure*/
	str c;                 /* Contact address */
	time_t expires;        /* expires parameter */
	float q;               /* q parameter */
	str callid;            /* Call-ID header field */
        int cseq;              /* CSeq value */
	struct ucontact* next; /* Next contact in the linked list */
	struct ucontact* prev; /* Previous contact in the linked list */
} ucontact_t;


/*
 * Create a new contact structure
 */
int new_ucontact(str* _aor, str* _contact, time_t _e, float _q, 
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
 * Update existing contact with new values
 */
int update_ucontact(ucontact_t* _c, time_t _e, float _q, str* _cid, int _cs);


/*
 * Delete contact from the database
 */
int db_del_ucontact(ucontact_t* _c);


/*
 * Update contact in the database
 */
int db_upd_ucontact(ucontact_t* _c);


/*
 * Insert contact into the database
 */
int db_ins_ucontact(ucontact_t* _c);


#endif /* UCONTACT_H */
