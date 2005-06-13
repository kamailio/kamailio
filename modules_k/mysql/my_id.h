/* 
 * $Id$
 *
 * Copyright (C) 2001-2004 iptel.org
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

#ifndef MY_ID_H
#define MY_ID_H

#include "../../str.h"

/*
 * All str fields must be zero terminated because we
 * will pass them to mysql_real_connect
 */
struct my_id {
	str username;  /* Username, case sensitive */
	str password;  /* Password, case sensitive */
	str host;      /* Host or IP, case insensitive */
	unsigned short port; /* Port number */
	str database;  /* Database, case sensitive */
};


/*
 * Create a new connection identifier
 */
struct my_id* new_my_id(const char* url);


/*
 * Compare two connection identifiers
 */
unsigned char cmp_my_id(struct my_id* id1, struct my_id* id2);


/*
 * Free a connection identifier
 */
void free_my_id(struct my_id* id);


#endif /* MY_ID_H */
