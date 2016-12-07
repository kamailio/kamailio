/*
 * Copyright (C) 2001-2004 FhG Fokus
 * Copyright (C) 2007-2008 1&1 Internet AG
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

/*!
 * \file lib/srdb1/db_cap.h
 * \ingroup db1
 * \brief Data structures that represents capabilities in the database.
 *
 * This file defines data structures that represents certain database
 * capabilities. It also provides some macros for convenient access to this
 * values.
 */

#ifndef DB1_CAP_H
#define DB1_CAP_H


/*! \brief
 * Represents the capabilities that a database driver supports.
 */
typedef enum db_cap {
	DB_CAP_QUERY =     1 << 0,  /*!< driver can perform queries                                     */
	DB_CAP_RAW_QUERY = 1 << 1,  /*!< driver can perform raw queries                                 */
	DB_CAP_INSERT =    1 << 2,  /*!< driver can insert data                                         */
	DB_CAP_DELETE =    1 << 3,  /*!< driver can delete data                                         */
	DB_CAP_UPDATE =    1 << 4,  /*!< driver can update data                                         */
	DB_CAP_REPLACE =   1 << 5,  /*!< driver can replace (also known as INSERT OR UPDATE) data       */
	DB_CAP_FETCH   =   1 << 6,  /*!< driver supports fetch result queries                           */
	DB_CAP_LAST_INSERTED_ID = 1 << 7,  /*!< driver can return the ID of the last insert operation   */
	DB_CAP_INSERT_UPDATE = 1 << 8, /*!< driver can insert data into database & update on duplicate  */
	DB_CAP_INSERT_DELAYED = 1 << 9, /*!< driver can do insert delayed                                */
	DB_CAP_AFFECTED_ROWS = 1 << 10 /*!< driver can return number of rows affected by the last query  */
} db_cap_t;


/*! \brief
 * All database capabilities except raw_query, replace, insert_update and 
 * last_inserted_id which should be checked separately when needed
 */
#define DB_CAP_ALL (DB_CAP_QUERY | DB_CAP_INSERT | DB_CAP_DELETE | DB_CAP_UPDATE)


/*! \brief
 * Returns true if all the capabilities in cpv are supported by module
 * represented by dbf, false otherwise
 */
#define DB_CAPABILITY(dbf, cpv) (((dbf).cap & (cpv)) == (cpv))


#endif /* DB1_CAP_H */
