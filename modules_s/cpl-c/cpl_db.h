/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 */

#ifndef _CPL_DB_H
#define _CPL_DB_H

#include "../../db/db.h"


/* inserts into database a cpl script in XML format(xml) along with its binary
 * format (bin)
 * Returns:  1 - success
 *          -1 - error
 */
int write_to_db(db_con_t *db_con, char *usr, str *xml, str *bin);


/* fetch from database the binary format of the cpl script for a given user
 * Returns:  1 - success
 *          -1 - error
 */
int get_user_script( db_con_t *db_hdl, str *user, str *script);


/* delete from database the entiry record for a given user - if a user has no
 * script, he will be removed complitly from db; users without script are not
 * allowed into db ;-)
 * Returns:  1 - success
 *          -1 - error
 */
int rmv_from_db(db_con_t *db_con, char *usr);


#endif
