/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

#ifndef _CPL_DB_H
#define _CPL_DB_H

#include "../../lib/srdb1/db.h"


int cpl_db_bind(const str* db_url, const str* db_table);
int cpl_db_init(const str* db_url, const str* db_table);
void cpl_db_close(void);

extern str cpl_username_col;
extern str cpl_domain_col;
extern str cpl_xml_col;
extern str cpl_bin_col;

/* inserts into database a cpl script in XML format(xml) along with its binary
 * format (bin)
 * Returns:  1 - success
 *          -1 - error
 */
int write_to_db(str *username, str*domain, str *xml, str *bin);


/* fetch from database the binary format of the cpl script for a given user
 * Returns:  1 - success
 *          -1 - error
 */
int get_user_script(str *username, str*domain, str *script, str *key);


/* delete from database the entire record for a given user - if a user has no
 * script, he will be removed completely from db; users without script are not
 * allowed into db ;-)
 * Returns:  1 - success
 *          -1 - error
 */
int rmv_from_db(str *username, str *domain);


#endif
