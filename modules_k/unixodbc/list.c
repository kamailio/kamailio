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


#include "list.h"

int insert(list l, int n, strn* value)
{
	int i;
	if(!l->next)
	{
		list ln;
		ln=(list)malloc(sizeof(element));
		if(!ln)
		{
			printf("list.c: No enought memory\n");
			return -1;
		}
		ln->next=NULL;
		ln->end=ln;
		l->next=ln;
		l->end=ln;
		ln->data=malloc(sizeof(strn)*n);
		for(i=0; i<n; i++)
			strcpy(ln->data[i].s, value[i].s);
	} else {
		insert(l->end, n, value);
	}
	return 0;
}

void destroy(list l)
{
	if(l->next)
		destroy(l->next);
	free(l);
}

strn* view(list l)
{
   return l->data;
}

void create(list *l, int n, strn* value)
{
	int i;
	if(*l)
	{
		printf("list.c: List already created\n");
		return;
	}
	*l=(list)malloc(sizeof(element));
	(*l)->next=NULL;
	(*l)->end=*l;
	(*l)->data=malloc(sizeof(strn)*n);
	for(i=0; i<n; i++)
		strcpy((*l)->data[i].s, value[i].s);
}

