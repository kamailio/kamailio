/*
 * $Id$
 *
 * Copyright (C) 2001-2004 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
	DB_CAP_UPDATE =    1 << 4,  /* Database driver can update data in the database */
	DB_CAP_REPLACE =   1 << 5,  /* Replace (also known as INSERT OR UPDATE) support */
	DB_CAP_FETCH   =   1 << 6,  /* Fetch result support */
} db_cap_t;


/*
 * All database capabilities except raw_query and replace which should be checked
 * separately when needed
 */
#define DB_CAP_ALL (DB_CAP_QUERY | DB_CAP_INSERT | DB_CAP_DELETE | DB_CAP_UPDATE)


/*
 * True if all the capabilities in cpv are supported by module
 * represented by dbf, false otherwise
 */
#define DB_CAPABILITY(dbf, cpv) (((dbf).cap & (cpv)) == (cpv))


#endif /* DB_CAP_H */
