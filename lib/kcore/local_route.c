/*
 *$Id: local_route.c 5132 2008-10-24 11:49:14Z miconda $
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 * \file
 * \brief Local Route related functions.
 */

#include <string.h>

#include "../../dprint.h"
#include "../../mem/mem.h"

#include "local_route.h"

static lrt_info_t* lrt_info_list = 0;
static int lrt_info_no = 0;


int lrt_do_init_child(void)
{
	int i;

	for ( i=0; i< lrt_info_no; i++ )
	{
		if ( lrt_info_list[i].init && lrt_info_list[i].init()!=0 )
		{
			LM_ERR("failed to init child for local route <%s>\n",
					 lrt_info_list[i].name);
			return -1;
		}
	}
	return 0;
}

int register_lrt_info(lrt_info_t *lrti)
{
	lrt_info_t *l;

	if(lrti==NULL || lrti->name==NULL || lrti->init==NULL)
		return 0;

	l = (lrt_info_t*)pkg_realloc(lrt_info_list,
			(lrt_info_no+1)*sizeof(lrt_info_t));
	if (l==0)
	{
		LM_ERR("no more pkg memory\n");
		return -1;
	}

	lrt_info_list = l;
	lrt_info_list[lrt_info_no].init = lrti->init;
	lrt_info_list[lrt_info_no].name = lrti->name;
	lrt_info_no++;

	return 0;
}


