/*
 * $Id$
 */

#ifndef AUTH_H
#define AUTH_H

#include "../../msg_parser.h"
#include "cred.h"


/*
 * The structure contains all data that need to be passed
 * among functions of authentication module, ie. from
 * authorize function to {www,proxy}_challenge functions
 */
typedef struct auth_state {
	     /* Parsed credentials */
	cred_t cred;

	     /* Indicates that last authentication attempt failed because of stale nonce */
	unsigned char stale;   

	     /* Number of retries obtained from nonce returned by client */
	int nonce_retries;
} auth_state_t;


extern auth_state_t state;


/*
 * Initialize authentication module
 */
void auth_init(void);


/*
 * Challenge a user agent, the first parameter is realm
 */
int www_challenge(struct sip_msg* _msg, char* _realm, char* _str2);

int proxy_challenge(struct sip_msg* _msg, char* _realm, char* _str2);


/*
 * Try to autorize request from a user, the first parameter
 * is realm
 */
int www_authorize(struct sip_msg* _msg, char* _realm, char* _str2);

int proxy_authorize(struct sip_msg* _msg, char* _realm, char* _str2);




#endif
