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
 */

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "list.h"

int insert(list l, int n, strn* value)
{
	int i;
	list ln;

	if(!l->next)
	{
		ln=(list)pkg_malloc(sizeof(element));
		if(!ln)
		{
			LOG(L_ERR,"ERROR:unixodbc:insert: No enought pkg memory (1)\n");
			return -1;
		}
		ln->data=pkg_malloc(sizeof(strn)*n);
		if(!ln->data)
		{
			LOG(L_ERR,"ERROR:unixodbc:insert: No enought pkg memory (2)\n");
			pkg_free(ln);
			return -1;
		}
		for(i=0; i<n; i++)
			strcpy(ln->data[i].s, value[i].s);
		/* link it */
		ln->next=NULL;
		ln->end=ln;
		l->next=ln;
		l->end=ln;
		
		return 0;
	}
	else
		return insert(l->end, n, value);
}

void destroy(list l)
{
	if(l->next)
		destroy(l->next);
	pkg_free(l->data);
	pkg_free(l);
}

strn* view(list l)
{
   return l->data;
}

int create(list *l, int n, strn* value)
{
	int i;
	if(*l)
	{
		LOG(L_WARN,"WARNING:unixodbc:create: List already created\n");
		return 0;
	}
	*l=(list)pkg_malloc(sizeof(element));
	if(!*l)
	{
		LOG(L_ERR,"ERROR:unixodbc:create: No enought pkg memory (1)\n");
		return -1;
	}
	(*l)->next=NULL;
	(*l)->end=*l;
	(*l)->data=pkg_malloc(sizeof(strn)*n);
	if(!(*l)->data)
	{
		LOG(L_ERR,"ERROR:unixodbc:create: No enought pkg memory (2)\n");
		pkg_free(*l);
		*l = 0;
		return -1;
	}
	for(i=0; i<n; i++)
		strcpy((*l)->data[i].s, value[i].s);

	return 0;
}

