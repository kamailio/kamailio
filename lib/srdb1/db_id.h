/* 
 * $Id$
 *
 * Copyright (C) 2001-2005 iptel.org
 * Copyright (C) 2007-2008 1&1 Internet AG
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

/**
 * \file db/db_id.h
 * \brief Functions for parsing a database URL and works with db identifier.
 */

#ifndef _DB_ID_H
#define _DB_ID_H

#include "../str.h"

/** Structure representing a database ID */
struct db_id {
	char* scheme;        /**< URL scheme */
	char* username;      /**< Username, case sensitive */
	char* password;      /**< Password, case sensitive */
	char* host;          /**< Host or IP, case insensitive */
	unsigned short port; /**< Port number */
	char* database;      /**< Database, case sensitive */
};


/**
 * Create a new connection identifier
 * \param url database URL
 * \return new allocated db_id structure, NULL on failure
 */
struct db_id* new_db_id(const str* url);


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


#endif /* _DB_ID_H */
