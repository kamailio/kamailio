/* 
 * Copyright (C) 2001-2005 iptel.org
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
 * \file lib/srdb1/db_id.h
 * \ingroup db1
 * \brief Functions for parsing a database URL and works with db identifier.
 */

#ifndef _DB1_ID_H
#define _DB1_ID_H

#include "../../str.h"
#include "db_pooling.h"

/** Structure representing a database ID */
struct db_id {
	str   url;           /**< full URL clone */
	char* scheme;        /**< URL scheme */
	char* username;      /**< Username, case sensitive */
	char* password;      /**< Password, case sensitive */
	char* host;          /**< Host or IP, case insensitive */
	unsigned short port; /**< Port number */
	char* database;      /**< Database, case sensitive */
	int   pid;           /**< Process ID (detect cross connections) */
	int   poolid;        /**< poolid within a pid */
};


/**
 * Create a new connection identifier
 * \param url database URL
 * \param pooling whether or not a pooled connection may be used
 * \return new allocated db_id structure, NULL on failure
 */
struct db_id* new_db_id(const str* url, db_pooling_t pooling);


/**
 * Compare two connection identifiers
 * \param id1 first identifier
 * \param id2 second identifier
 * \return 1 if both identifier are equal, 0 if there not equal
 */
unsigned char cmp_db_id(const struct db_id* id1, const struct db_id* id2);


/**
 * Free a connection identifier
 * \param id the identifier that should released
 */
void free_db_id(struct db_id* id);


#endif /* _DB1_ID_H */
