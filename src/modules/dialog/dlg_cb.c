/*
 * Copyright (C) 2006 Voice Sistem SRL
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


/*!
 * \file
 * \brief Callbacks
 * \ingroup dialog
 * Module: \ref dialog
 */


#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "dlg_hash.h"
#include "dlg_cb.h"


static struct dlg_head_cbl* create_cbs = 0;

static struct dlg_head_cbl* load_cbs = 0;

static struct dlg_cb_params params = {NULL, NULL, DLG_DIR_NONE, NULL, NULL};


#define POINTER_CLOSED_MARKER  ((void *)(-1))


static void run_load_callback(struct dlg_callback *cb);



static struct dlg_head_cbl* init_dlg_callback(void)
{
	struct dlg_head_cbl *new_cbs;

	new_cbs = (struct dlg_head_cbl*)shm_malloc(sizeof(struct dlg_head_cbl));
	if (new_cbs==0) {
		LM_ERR("no more shm mem\n");
		return 0;
	}
	new_cbs->first = 0;
	new_cbs->types = 0;

	return new_cbs;
}


void destroy_dlg_callbacks_list(struct dlg_callback *cb)
{
	struct dlg_callback *cb_t;

	while(cb) {
		cb_t = cb;
		cb = cb->next;
		if (cb_t->callback_param_free && cb_t->param) {
			cb_t->callback_param_free(cb_t->param);
			cb_t->param = NULL;
		}
		shm_free(cb_t);
	}
}


void destroy_dlg_callbacks(unsigned int types)
{
	if (types&DLGCB_CREATED) {
		if (create_cbs && create_cbs!=POINTER_CLOSED_MARKER) {
			destroy_dlg_callbacks_list(create_cbs->first);
			shm_free(create_cbs);
			create_cbs = POINTER_CLOSED_MARKER;
		}
	}
	if (types&DLGCB_LOADED) {
		if (load_cbs && load_cbs!=POINTER_CLOSED_MARKER) {
			destroy_dlg_callbacks_list(load_cbs->first);
			shm_free(load_cbs);
			load_cbs = POINTER_CLOSED_MARKER;
		}
	}
}


int register_dlgcb(struct dlg_cell *dlg, int types, dialog_cb f,
										void *param, param_free_cb ff )
{
	struct dlg_callback *cb;

	if ( types&DLGCB_LOADED ) {
		if (types!=DLGCB_LOADED) {
			LM_CRIT("DLGCB_LOADED type must be register alone!\n");
			return -1;
		}
	} else if ( types&DLGCB_CREATED ) {
		if (types!=DLGCB_CREATED) {
			LM_CRIT("DLGCB_CREATED type must be register alone!\n");
			return -1;
		}
	} else {
		if (dlg==0) {
			LM_CRIT("non-DLGCB_CREATED type "
				"must be register to a dialog (dlg missing)!\n");
			return -1;
		}
	}
	cb = (struct dlg_callback*)shm_malloc(sizeof(struct dlg_callback));
	if (cb==0) {
		LM_ERR("no more shm mem\n");
		return -1;
	}

	cb->types = types;
	cb->callback = f;
	cb->param = param;
	cb->callback_param_free = ff;

	if ( types==DLGCB_CREATED ) {
		if (load_cbs==POINTER_CLOSED_MARKER) {
			LM_CRIT("DLGCB_CREATED type registered after shutdown!?!\n");
			goto error;
		}
		if (create_cbs==0) {
			/* not initialized yet */
			if ( (create_cbs=init_dlg_callback())==NULL ) {
				LM_ERR("no more shm mem\n");
				goto error;
			}
		}
		cb->next = create_cbs->first;
		create_cbs->first = cb;
		create_cbs->types |= types;
	} else if (types==DLGCB_LOADED) {
		if (load_cbs==POINTER_CLOSED_MARKER) {
			/* run the callback on the spot */
			run_load_callback(cb);
			destroy_dlg_callbacks_list(cb);
			return 0;
		}
		if (load_cbs==0) {
			/* not initialized yet */
			if ( (load_cbs=init_dlg_callback())==NULL ) {
				LM_ERR("no more shm mem\n");
				goto error;
			}
		}
		cb->next = load_cbs->first;
		load_cbs->first = cb;
		load_cbs->types |= types;
	} else {
		cb->next = dlg->cbs.first;
		dlg->cbs.first = cb;
		dlg->cbs.types |= types;
	}

	return 0;
error:
	shm_free(cb);
	return -1;
}


static void run_load_callback(struct dlg_callback *cb)
{
	struct dlg_cell *dlg;
	unsigned int i;

	params.req = NULL;
	params.rpl = NULL;
	params.direction = DLG_DIR_NONE;
	params.param = &cb->param;

	for( i=0 ; i<d_table->size ; i++ ) {
		for( dlg=d_table->entries[i].first ; dlg ; dlg=dlg->next )
			cb->callback( dlg, DLGCB_LOADED, &params );
	}

	return;
}


void run_load_callbacks( void )
{
	struct dlg_callback *cb;

	if (load_cbs && load_cbs!=POINTER_CLOSED_MARKER) {
		for ( cb=load_cbs->first; cb; cb=cb->next )
			run_load_callback( cb );
	}

	return;
}


void run_create_callbacks(struct dlg_cell *dlg, struct sip_msg *msg)
{
	struct dlg_callback *cb;

	if (create_cbs==NULL || create_cbs->first==NULL)
		return;

	params.req = msg;
	params.rpl = NULL;
	/* initial request goes DOWNSTREAM all the time */
	params.direction = DLG_DIR_DOWNSTREAM;
	/* avoid garbage due static structure */
	params.param = NULL;
	params.dlg_data = NULL;

	for ( cb=create_cbs->first; cb; cb=cb->next)  {
		LM_DBG("dialog=%p\n",dlg);
		params.param = &cb->param;
		cb->callback( dlg, DLGCB_CREATED, &params );
	}
	return;
}


void run_dlg_callbacks( int type ,
						struct dlg_cell *dlg,
						struct sip_msg *req,
						struct sip_msg *rpl,
						unsigned int dir, void *dlg_data)
{
	struct dlg_callback *cb;

	params.req = req;
	params.rpl = rpl;
	params.direction = dir;
	params.dlg_data = dlg_data;

	if (dlg->cbs.first==0 || ((dlg->cbs.types)&type)==0 )
		return;

	for ( cb=dlg->cbs.first; cb; cb=cb->next)  {
		if ( (cb->types)&type ) {
			LM_DBG("dialog=%p, type=%d\n", dlg, type);
			params.param = &cb->param;
			cb->callback( dlg, type, &params );
		}
	}
	return;
}
