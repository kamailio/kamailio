/* 
 * $Id$ 
 *
 * UNIXODBC module
 *
 * Copyright (C) 2005-2006 Marco Lorrai
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
 *
 *
 * History:
 * --------
 *  2005-12-01  initial commit (chgen)
 *  2006-04-04  simplified link list (sgupta)
 */

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "list.h"

int insert(list** start, list** link, int n, strn* value)
{
	int i = 0;

	if(!(*start)) {
		*link = (list*)pkg_malloc(sizeof(list));
		if(!(*link)) {
			LOG(L_ERR,"ERROR:unixodbc:insert: Not enough pkg memory (1)\n");
			return -1;
		}
		(*link)->next = NULL;
		(*link)->data = pkg_malloc(sizeof(strn)*n);
		if(!(*link)->data) {
			LOG(L_ERR,"ERROR:unixodbc:insert: Not enough pkg memory (2)\n");
			pkg_free(*link);
			*link = NULL;
			return -1;
		}
		for(i=0; i<n; i++)
			strcpy((*link)->data[i].s, value[i].s);
	
		*start = *link;
		return 0;
	}
	else
	{
		list* nlink;
		nlink=(list*)pkg_malloc(sizeof(list));
		if(!nlink) {
			LOG(L_ERR,"ERROR:unixodbc:insert: Not enough pkg memory (3)\n");
			return -1;
		}
		nlink->data = pkg_malloc(sizeof(strn)*n);
		if(!nlink->data) {
			LOG(L_ERR,"ERROR:unixodbc:insert: Not enough pkg memory (4)\n");
			pkg_free(nlink);
			return -1;
		}
		for(i=0; i<n; i++)
			strcpy(nlink->data[i].s, value[i].s);

		nlink->next = NULL;
		(*link)->next = nlink;
		*link = (*link)->next;

		return 0;
	}
}


void destroy(list *start)
{
	while(start) {
		list* temp = start;
		start = start->next;
		pkg_free(temp->data);
		pkg_free(temp);
	}
}

