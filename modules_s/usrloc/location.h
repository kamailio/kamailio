/* 
 * $Id$ 
 */

#ifndef LOCATION_H
#define LOCATION_H

#include <time.h>
#include "contact.h"
#include "../../parser/msg_parser.h"
#include "../../db/db.h"


/*
 * The structure holds all information
 * associated with one Address Of Record
 */
typedef struct location {
	str user;             /* Address of record          */
	contact_t* contacts;  /* One or more contact fields */
} location_t;


/* Address of record */
#define LOC_AOR(loc) ((loc)->user)

/* Address of record string */
#define LOC_AOR_STR(loc) ((loc)->user.s)

/* Address of record length */
#define LOC_AOR_LEN(loc) ((loc)->user.len)

/* First contact in the linked list */
#define LOC_CONTACTS_FIRST(loc) ((loc)->contacts)

/* Next contact in the linked list */
#define LOC_CONTACTS_NEXT(con) ((con)->next)

/* Contacts == NULL */
#define IS_EMPTY(loc) (((loc)->contacts) == NULL)


/*
 * Convert REGISTER SIP message into location
 */
int sip_to_loc(struct sip_msg* _msg, location_t** _loc, int* _star, int* _expires);

/*
 * Create a new location structure
 */
int create_location(location_t** _loc, str* _user);

/*
 * Add a contact into existing location structure
 */
int add_contact(location_t* _loc, const char* _contact, time_t _expires, float _q,
		const char* _callid, int _cseq);

/*
 * Remove contact from existing location structure
 */
int remove_contact(location_t* _loc, const char* _contact);

/*
 * print location structure, for debuggin purpose only
 */
void print_location(const location_t* _loc);

/*
 * Free all memory associated with the given location structure
 */
void free_location(location_t* _loc);

/*
 * Compare Address of record of a location with given string
 */
int cmp_location(location_t* _loc, const char* _aor);


/*
 * Check if the source message has been formed correctly
 */
int validate_location(location_t* _loc, int _expires, int _star, int* _result);


/*
 * Remove all contacts, that have expired already
 */
int remove_zero_expires(location_t* _loc);


/*
 * Insert a location into database
 */
int db_insert_location(db_con_t* _c, location_t* _loc);


/*
 * Remove location from database
 */
int db_remove_location(db_con_t* _c, location_t* _loc);


/*
 * Update bindings that belong to location
 */
int update_location(db_con_t* _c, location_t* _dest, location_t* _src, int* _sr);


/*
 * Remove expired bindings
 */
int clean_location(location_t* _l, db_con_t* _c, time_t _t);


#endif
