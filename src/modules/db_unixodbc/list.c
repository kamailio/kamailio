/*
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

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "list.h"


/*!
 * \brief Create a list
 * \param start start of the list
 * \param link inserted element
 * \param n number of values
 * \param value inserted value
 * \return 0 on success, -1 on failure
 */
int db_unixodbc_list_insert(list** start, list** link, int n, strn* value)
{
	int i = 0;
	list* nlink;

	if (!(*start)) *link = NULL;

	nlink=(list*)pkg_malloc(sizeof(list));
	if(!nlink) {
		LM_ERR("no more pkg memory (1)\n");
		return -1;
	}
	nlink->rownum = n;
	nlink->next = NULL;

	nlink->lengths = (unsigned long*)pkg_malloc(sizeof(unsigned long)*n);
	if(!nlink->lengths) {
		LM_ERR("no more pkg memory (2)\n");
		pkg_free(nlink);
		return -1;
	}
	for(i=0; i<n; i++)
		nlink->lengths[i] = value[i].buflen;

	nlink->data = (char**)pkg_malloc(sizeof(char*)*n);
	if(!nlink->data) {
		LM_ERR("no more pkg memory (3)\n");
		pkg_free( nlink->lengths );
		pkg_free(nlink);
		return -1;
	}

	for(i=0; i<n; i++) {
		nlink->data[i] = pkg_malloc(sizeof(char) * nlink->lengths[i]);
		if(!nlink->data[i]) {
			LM_ERR("no more pkg memory (4)\n");
			pkg_free( nlink->lengths );
			pkg_free( nlink->data );
			pkg_free(nlink);
			return -1;
		}
		memcpy(nlink->data[i], value[i].s, nlink->lengths[i]);
	}

	if (!(*start)) {
		*link = nlink;
		*start = *link;
	} else {
		(*link)->next = nlink;
		*link = (*link)->next;
	}

	return 0;
}

/*!
 * \brief Destroy a list
 * \param link list element(s)
 */
void db_unixodbc_list_destroy(list *start)
{
	int i = 0;

	while(start) {
		list* temp = start;
		start = start->next;
		for(i = 0; i < temp->rownum; i++)
			pkg_free( temp->data[i] );
		pkg_free(temp->data);
		pkg_free(temp->lengths);
		pkg_free(temp);
	}
}

