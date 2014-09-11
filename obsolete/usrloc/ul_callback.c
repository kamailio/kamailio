/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*
 * History:
 * --------
 *  2004-03-16  created (bogdan)
 */



#include <stdlib.h>

#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/shm_mem.h"
#include "ul_callback.h"


struct ulcb_head_list* ulcb_list = 0;



int init_ulcb_list()
{
	ulcb_list = (struct ulcb_head_list*)shm_malloc
		( sizeof(struct ulcb_head_list) );
	if (ulcb_list==0) {
		LOG(L_CRIT,"ERROR:usrloc:init_ulcb_list: no more shared mem\n");
		return -1;
	}
	ulcb_list->first = 0;
	ulcb_list->reg_types = 0;
	return 1;
}


void destroy_ulcb_list()
{
	struct ul_callback *cbp, *cbp_tmp;

	if (!ulcb_list)
		return;

	for( cbp=ulcb_list->first; cbp ; ) {
		cbp_tmp = cbp;
		cbp = cbp->next;
		if (cbp_tmp->param) shm_free( cbp_tmp->param );
		shm_free( cbp_tmp );
	}

	shm_free(ulcb_list);
}



/* register a callback function 'f' for 'types' mask of events;
*/
int register_ulcb( int types, ul_cb f, void *param )
{
	struct ul_callback *cbp;

	/* are the callback types valid?... */
	if ( types<0 || types>ULCB_MAX ) {
		LOG(L_CRIT, "BUG:usrloc:register_ulcb: invalid callback types: "
			"mask=%d\n",types);
		return E_BUG;
	}
	/* we don't register null functions */
	if (f==0) {
		LOG(L_CRIT, "BUG:usrloc:register_ulcb: null callback function\n");
		return E_BUG;
	}

	/* build a new callback structure */
	if (!(cbp=(struct ul_callback*)shm_malloc(sizeof( struct ul_callback)))) {
		LOG(L_ERR, "ERROR:usrloc:register_ulcb: out of shm. mem\n");
		return E_OUT_OF_MEM;
	}

	/* link it into the proper place... */
	cbp->next = ulcb_list->first;
	ulcb_list->first = cbp;
	ulcb_list->reg_types |= types;
	/* ... and fill it up */
	cbp->callback = f;
	cbp->param = param;
	cbp->types = types;
	if (cbp->next)
		cbp->id = cbp->next->id+1;
	else
		cbp->id = 0;

	return 1;
}



