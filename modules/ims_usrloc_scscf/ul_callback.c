/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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
 *  2004-03-16  created (bogdan)
 */

/*! \file
 *  \brief USRLOC - Callback functions
 *  \ingroup usrloc
 *
 * - Module: \ref usrloc
 */


#include <stdlib.h>

#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/shm_mem.h"
#include "ul_callback.h"
#include "ucontact.h"

struct ulcb_head_list* ulcb_list = 0;



int init_ulcb_list(void)
{
	ulcb_list = (struct ulcb_head_list*)shm_malloc
		( sizeof(struct ulcb_head_list) );
	if (ulcb_list==0) {
		LM_CRIT("no more shared mem\n");
		return -1;
	}
	ulcb_list->first = 0;
	ulcb_list->reg_types = 0;
	return 1;
}


void destroy_ulcb_list(void)
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



/*! \brief 
	register a callback function 'f' for 'types' mask of events;
*/
int register_ulcb( struct impurecord* r, struct ucontact* c, int types, ul_cb f, void *param )
{
	struct ul_callback *cbp;
	int contact_cb=0;
	int impurecord_cb=0;

	struct ulcb_head_list* cb_list=0;

	if (types & UL_IMPU_INSERT) {
		if (types != UL_IMPU_INSERT) {
			LM_CRIT("UL_IMPU_INSERT type must be registered alone!\n");
			return -1;
		}
		cb_list = ulcb_list;
	} else if (types & UL_CONTACT_INSERT) {
		if (types != UL_CONTACT_INSERT) {
			LM_CRIT("UL_CONTACT_INSERT type must be registered alone!\n");
			return -1;
		}
		cb_list = ulcb_list;
	} else {
		contact_cb = (types & UL_CONTACT_DELETE)
				|| (types & UL_CONTACT_EXPIRE) || (types & UL_CONTACT_UPDATE);
		impurecord_cb = (types & UL_IMPU_DELETE)
				|| (types & UL_IMPU_UPDATE)
				|| (types & UL_IMPU_REG_NC_DELETE)
				|| (types & UL_IMPU_NR_DELETE)
				|| (types & UL_IMPU_UNREG_EXPIRED)
				|| (types & UL_IMPU_UPDATE_CONTACT)
                                || (types & UL_IMPU_DELETE_CONTACT)
                                || (types & UL_IMPU_EXPIRE_CONTACT)
                                || (types & UL_IMPU_NEW_CONTACT);
                

		if (contact_cb && impurecord_cb) {
			LM_CRIT("can't register IMPU and Contact callback in same call\n");
			return -1;
		}
		if (contact_cb && !c) {
			LM_CRIT("no contact provided for contact callback\n");
			return -1;
		}
		if (impurecord_cb && !r) {
			LM_CRIT("no impurecord provided for contact callback\n");
			return -1;
		}
	}

	/* are the callback types valid?... */
	if ( types<0 || types>ULCB_MAX ) {
		LM_CRIT("invalid callback types: mask=%d\n",types);
		return E_BUG;
	}
	/* we don't register null functions */
	if (f==0) {
		LM_CRIT("null callback function\n");
		return E_BUG;
	}

	if (contact_cb) {
		LM_DBG("installing callback for SCSCF contact with type [%d]\n", types);
		cb_list = c->cbs;
	} else if (impurecord_cb) {
		LM_DBG("installing callback for SCSCF IMPU record with type [%d]\n", types);
		cb_list = r->cbs;
	}

	/* build a new callback structure */
	if (!(cbp=(struct ul_callback*)shm_malloc(sizeof( struct ul_callback)))) {
		LM_ERR("no more share mem\n");
		return E_OUT_OF_MEM;
	}

	/* link it into the proper place... */
	cbp->next = cb_list->first;
	cb_list->first = cbp;
	cb_list->reg_types |= types;
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



