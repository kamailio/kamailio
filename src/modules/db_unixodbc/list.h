/* 
 * $Id$ 
 *
 * UNIXODBC module
 *
 * Copyright (C) 2005-2006 Marco Lorrai
 * Copyright (C) 2008 1&1 Internet AG
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
 *
 * History:
 * --------
 *  2005-12-01  initial commit (chgen)
 *  2006-04-04  simplified link list (sgupta)
 *  2006-05-05  removed static allocation of 1k per column data (sgupta)
 */

#ifndef _UNIXODBC_LIST_H_
#define _UNIXODBC_LIST_H_

#include <stdio.h>
#include <stdlib.h>
#include "connection.h"


typedef struct list
{
	struct list* next;
	char** data;
	unsigned long* lengths;
	int rownum;
} list;


/*!
 * \brief Create a list
 * \param start start of the list
 * \param link inserted element
 * \param n number of values
 * \param value inserted value
 * \return 0 on success, -1 on failure
 */
int db_unixodbc_list_insert(list** start, list** link, int n, strn* value);


/*!
 * \brief Destroy a list
 * \param link list element(s)
 */
void db_unixodbc_list_destroy(list* link);

#endif
