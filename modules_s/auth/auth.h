/*
 * $Id$
 */

#ifndef AUTH_H
#define AUTH_H

#include "../../msg_parser.h"


/*
 * Function prototypes
 */
void auth_init(void);


/*
 * Challenge a user agent, the first parameter is realm
 */
int challenge(struct sip_msg* _msg, char* _realm, char* _str2);


/*
 * Try to autorize request from a user, the first parameter
 * is realm
 */
int authorize(struct sip_msg* _msg, char* _realm, char* _str2);

#endif
