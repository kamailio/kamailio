/*
 * $Id$
 *
 * Authorize related functions
 */

#ifndef AUTHORIZE_H
#define AUTHORIZE_H


#include "../../parser/msg_parser.h"


/*
 * Authorize using Proxy-Authorization header field
 */
int radius_proxy_authorize(struct sip_msg* _msg, char* _realm, char* _table);


/*
 * Authorize using WWW-Authorization header field
 */
int radius_www_authorize(struct sip_msg* _msg, char* _realm, char* _table);


/*
 * Remove used credentials
 */
int consume_credentials(struct sip_msg* _msg, char* _s1, char* _s2);


#endif /* AUTHORIZE_H */
