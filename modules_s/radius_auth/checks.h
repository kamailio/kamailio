/*
 * $Id$
 *
 * Check if to and from contain the same username as
 * in digest credentials
 */

#ifndef CHECKS_H
#define CHECKS_H

#include "../../parser/msg_parser.h"


/*
 * Check if To header field contains the same username
 * as digest credentials
 */
int check_to(struct sip_msg* _msg, char* _str1, char* _str2);


/*
 * Check if From header field contains the same username
 * as digest credentials
 */
int check_from(struct sip_msg* _msg, char* _str1, char* _str2);


#endif /* CHECKS_H */
