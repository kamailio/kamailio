/*
 * $Id$
 *
 * Check, if a username matches those in digest credentials
 * or if a user is member of a group
 */

#ifndef GROUP_H
#define GROUP_H

#include "../../parser/msg_parser.h"


/*
 * Check if given username matches those in digest credentials
 */
int is_user(struct sip_msg* _msg, char* _user, char* _str2);


/*
 * Check if a user is member of a group
 */
int is_in_group(struct sip_msg* _msg, char* _group, char* _str2);


/*
 * Check if user specified in credentials is in a table
 */
int is_user_in(struct sip_msg* _msg, char* _table, char* _s);


#endif /* GROUP_H */
