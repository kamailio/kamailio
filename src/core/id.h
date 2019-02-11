/*
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _ID_H
#define _ID_H

#include "str.h"
#include "usr_avp.h"
#include "parser/msg_parser.h"


/*
 * Set From UID
 */
int set_from_uid(str* uid);


/*
 * Get From UID
 */
int get_from_uid(str* uid, struct sip_msg* msg);

/*
 * Set To UID
 */
int set_to_uid(str* uid);


/*
 * Ge To UID
 */
int get_to_uid(str* uid, struct sip_msg* msg);


/** Retrieves the UID of the callee. This function retrieves the UID (unique
 * identifier) of the party being called. The function first searches the list
 * of available attributes and if it finds an attribute with name "uid" then
 * the value of the attribute is returned.  If no such attribute can be found
 * then the function retrieves the username from To header field of REGISTER
 * requests (because that is the party being registered), or the username from
 * the Reqeuest-URI of other requests. The username is then used as the UID
 * string identifying the callee. If no attribute with the UID was found and
 * the function successfully retrieved the UID from the SIP message then, in
 * addition to storing the result in the first parameter, the function will
 * also create the attribute named "uid" which will contain the UID. The
 * function is not reentrant because it uses an internal static buffer to
 * store the result.
 * @param uid A pointer to ::str variable where the result will be stored, the
 *            pointer in the variable will be updated to point to a static
 *            buffer in the function.  
 * @param msg The SIP message being processed.  
 * @return 1 is returned when the attribute with UID exists and it is used, 0
 *         is returned when the function retrieved the UID from the SIP
 *         message and created the attribute, -1 is returned on error.
 */
int get_to_did(str* did, struct sip_msg* msg);


/*
 * Return current From domain id
 */
int get_from_did(str* did, struct sip_msg* msg);


#endif /* _ID_H */
