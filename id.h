/*
 * $Id$
 *
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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


/*
 * Return current To domain id
 */
int get_to_did(str* did, struct sip_msg* msg);


/*
 * Return current From domain id
 */
int get_from_did(str* did, struct sip_msg* msg);


#endif /* _ID_H */
