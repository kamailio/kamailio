/* 
 * $Id$ 
 */

#ifndef __LOCATION_H__
#define __LOCATION_H__

#include <time.h>
#include "contact.h"
#include "../../msg_parser.h"
#include "db.h"


/*
 * The structure holds all information
 * associated with one Address Of Record
 */
typedef struct location {
	str user;             /* Address of record          */
	contact_t* contacts;  /* One or more contact fields */
} location_t;

/*
 * Convert REGISTER SIP message into location
 */
int msg2loc(struct sip_msg* _msg, location_t** _loc, int* _star, int* _expires);

/*
 * Create a new location structure
 */
int create_location(location_t** _loc, const char* _user);

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


int remove_zero_expires(location_t* _loc);


/*
 * ============================ DB related functions ====================
 */


/*
 * Insert a location into database
 */
int db_insert_location(db_con_t* _c, location_t* _loc);


int db_remove_location(db_con_t* _c, location_t* _loc);


int update_location(db_con_t* _c, location_t* _dest, location_t* _src);

#endif
