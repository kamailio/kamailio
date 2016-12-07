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
 */

#include <stdlib.h>

#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/shm_mem.h"
#include "ul_callback.h"
#include "usrloc.h"

struct ulcb_head_list* ulcb_list = 0;			/*<! list for create callbacks */

int init_ulcb_list(void)
{
	ulcb_list = (struct ulcb_head_list*)shm_malloc( sizeof(struct ulcb_head_list) );
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

void destroy_ul_callbacks_list(struct ul_callback* cb) {

	struct ul_callback *cb_t;

	while (cb) {
		cb_t = cb;
		cb = cb->next;
//		if (cb_t->callback_param_free && cb_t->param) {
//			cb_t->callback_param_free(cb_t->param);
//			cb_t->param = NULL;
//		} //TODO: this is if we need/require a freeparam function
		shm_free(cb_t);
	}
}

int register_ulcb( struct pcontact *c, int types, ul_cb f, void *param )
{
	struct ul_callback *cbp;

	/* are the callback types valid?... */
	if ( types<0 || types>PCSCF_MAX ) {
		LM_CRIT("invalid callback types: mask=%d\n",types);
		return E_BUG;
	}
	/* we don't register null functions */
	if (f==0) {
		LM_CRIT("null callback function\n");
		return E_BUG;
	}

	/* build a new callback structure */
	if (!(cbp=(struct ul_callback*)shm_malloc(sizeof( struct ul_callback)))) {
		LM_ERR("no more share mem\n");
		return E_OUT_OF_MEM;
	}

	cbp->callback = f;
	cbp->param = param;
	cbp->types = types;

	if ( types==PCSCF_CONTACT_INSERT ) {
		LM_DBG("TODO: check for registering callback before/after init\n");
		/* link it into the proper place... */
		cbp->next = ulcb_list->first;
		ulcb_list->first = cbp;
		ulcb_list->reg_types |= types;
	} else {
		/* link it into pcontact structure */
		cbp->next = c->cbs.first;
		c->cbs.first = cbp;
		c->cbs.reg_types |= types;
	}

	return 1;
}

/*! \brief run all transaction callbacks for an event type */
void run_ul_callbacks( int type , struct pcontact *c)
{
	struct ul_callback *cbp;

	if (c->cbs.first == 0 || ((c->cbs.reg_types) & type) == 0)
		return;

	for (cbp=c->cbs.first; cbp; cbp=cbp->next) {
		if ((cbp->types) & type) {
			LM_DBG("contact=%p, callback type %d/%d entered\n", c, type, cbp->types);
			cbp->callback( c, type, cbp->param );
		}
	}
	return;
}

void run_ul_create_callbacks(struct pcontact *c)
{
	struct ul_callback *cbp;

	for (cbp = ulcb_list->first; cbp; cbp = cbp->next) {
			LM_DBG("contact=%p, callback type PCSCF_CONTACT_INSERT entered\n", c);
		cbp->callback(c, PCSCF_CONTACT_INSERT, cbp->param);
	}
}

