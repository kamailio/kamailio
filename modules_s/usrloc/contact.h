/* 
 * $Id$ 
 */

#ifndef __CONTACT_H__
#define __CONTACT_H__

#include <time.h>
#include "../../str.h"
#include "db.h"


typedef struct contact {
	str* aor;              /* Pointer to the address of record string in location structure*/
	str c;                 /* Contact address */
	time_t expires;        /* expires parameter */
	float q;               /* q parameter */
	char* callid;    /* Call-ID header field */
        int cseq;              /* CSeq value */
	struct contact* next;  /* Next contact in the linked list */
} contact_t;


/*
 * Print contact, for debugging purposes only
 */
void print_contact(contact_t* _c);


/*
 * Create a new contact structure
 */
int create_contact(contact_t** _con, str* _aor, const char* _c, time_t _expires, float _q,
		   const char* _callid, int _cseq);


/*
 * Free all memory associated with given contact structure
 */
void free_contact(contact_t* _c);


int update_contact(contact_t* _dst, contact_t* _src);

/*
 * Compare contacts
 */
int cmp_contact(contact_t* _c1, contact_t* _c2);


/*
 * Remove particular contact from database
 */
int db_remove_contact(db_con_t* _c, contact_t* _con);


/*
 * Update particular contact in database
 */
int db_update_contact(db_con_t* _c, contact_t* _con);


#endif
