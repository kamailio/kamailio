/*
 * pua module - presence user agent module
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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


#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/shm_mem.h"
#include "pua_callback.h"


struct puacb_head_list* puacb_list = 0;

int init_puacb_list(void)
{
	puacb_list = (struct puacb_head_list*)shm_malloc
		( sizeof(struct puacb_head_list) );
	if (puacb_list==0)
	{
		LM_CRIT("no more shared mem\n");
		return -1;
	}
	puacb_list->first = 0;
	puacb_list->reg_types = 0;
	return 1;
}


void destroy_puacb_list(void)
{
	struct pua_callback *cbp, *cbp_tmp;

	if (!puacb_list)
		return;

	for( cbp=puacb_list->first; cbp ; )
	{
		cbp_tmp = cbp;
		cbp = cbp->next;
		if (cbp_tmp->param) 
			shm_free( cbp_tmp->param );
		shm_free( cbp_tmp );
	}
	shm_free(puacb_list);
}



/* register a callback function 'f' for 'types' mask of events;
*/
int register_puacb( int types, pua_cb f, void* param )
{
	struct pua_callback *cbp;

	/* are the callback types valid?... */
	if ( types<0 || types>PUACB_MAX ) 
	{
		LM_CRIT("invalid callback types: mask=%d\n",types);
		return E_BUG;
	}
	/* we don't register null functions */
	if (f==0)
	{
		LM_CRIT("null callback function\n");
		return E_BUG;
	}

	/* build a new callback structure */
	if (!(cbp=(struct pua_callback*)shm_malloc(sizeof( struct pua_callback)))) 
	{
		LM_ERR("out of share mem\n");
		return E_OUT_OF_MEM;
	}

	/* link it into the proper place... */
	cbp->next = puacb_list->first;
	puacb_list->first = cbp;
	puacb_list->reg_types |= types;

	cbp->callback = f;
    cbp->param= param;
	cbp->types = types;
	if (cbp->next)
		cbp->id = cbp->next->id+1;
	else
		cbp->id = 0;

	return 1;
}

