/*
 * $Id$
 *
 * Copyright (C) 2001-2004 FhG Fokus
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

#ifndef DB_CAP_H
#define DB_CAP_H


/*
 * Database capabilities
 */
typedef enum db_cap {
	DB_CAP_QUERY =     1 << 0,  /* Database driver can query database */
	DB_CAP_RAW_QUERY = 1 << 1,  /* Database driver can perform raw queries */
	DB_CAP_INSERT =    1 << 2,  /* Database driver can insert data into database */
	DB_CAP_DELETE =    1 << 3,  /* Database driver can delete data from database */
	DB_CAP_UPDATE =    1 << 4   /* Database driver can update data in the database */
} db_cap_t;


/*
 * All database functions except raw_query
 */
#define DB_CAP_ALL (DB_CAP_QUERY | DB_CAP_INSERT | DB_CAP_DELETE | DB_CAP_UPDATE)	


/*
 * True if all the capabilities in cpv are supported by module
 * represented by dbf, false otherwise
 */
#define DB_CAPABILITY(dbf, cpv) ((dbf)->cap & (cpv)) == (cpv))


#endif /* DB_CAP_H */
