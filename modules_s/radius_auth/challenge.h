/*
 * $Id$
 *
 * Challenge related functions
 */

#ifndef CHALLENGE_H
#define CHALLENGE_H

#include "../../parser/msg_parser.h"


/* 
 * Challenge a user agent using WWW-Authenticate header field
 */
int radius_www_challenge(struct sip_msg* _msg, char* _realm, char* _str2);


/*
 * Challenge a user agent using Proxy-Authenticate header field
 */
int radius_proxy_challenge(struct sip_msg* _msg, char* _realm, char* _str2);


#endif /* CHALLENGE_H */
