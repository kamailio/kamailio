/* 
 * $Id$ 
 */

#ifndef __LOCATION_H__
#define __LOCATION_H__

#include <time.h>
#include "contact.h"
#include "../../msg_parser.h"


typedef struct location {
	str user;            /* Address of record */
	unsigned char star;   /* 1 if it is * contact field */
	contact_t* contacts; /* One or more contact fields */
	time_t expires;      /* Expiry time of the record */
} location_t;

location_t* msg2loc  (struct sip_msg* _msg);
location_t* create_location (const char* _user, int _star, time_t _expires);
int         add_contact     (location_t* _loc, const char* _contact, time_t _expire, float _q,
			     unsigned char _new, unsigned char _dirty);
int         remove_contact  (location_t* _loc, const char* _contact);
void        print_location  (const location_t* _loc);
void        free_location   (location_t* _loc);

int         cmp_location    (location_t* _loc, const char* _aor);
int         merge_location (location_t* _new, const location_t* _old);

#endif
