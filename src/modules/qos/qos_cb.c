/*
 * Copyright (C) 2007 SOMA Networks, Inc.
 * Written by Ovidiu Sas (osas)
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


#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "qos_ctx_helpers.h"
#include "qos_cb.h"

static struct qos_head_cbl* create_cbs = 0;

static struct qos_cb_params params = {NULL, NULL, 0, NULL};

int init_qos_callbacks(void)
{
	create_cbs = (struct qos_head_cbl*)shm_malloc(sizeof(struct qos_head_cbl));
	if (create_cbs==0) {
		LM_ERR("no more shm mem\n");
		return -1;
	}
	create_cbs->first = 0;
	create_cbs->types = 0;
	return 0;
}


void destroy_qos_callbacks_list(struct qos_callback *cb)
{
	struct qos_callback *cb_t;

	while(cb) {
		cb_t = cb;
		cb = cb->next;
		/* FIXME - what about parameters ? */
		LM_DBG("freeing cp=%p\n", cb_t);
		shm_free(cb_t);
	}
}


void destroy_qos_callbacks(void)
{
	if (create_cbs==0)
		return;

	destroy_qos_callbacks_list(create_cbs->first);
	shm_free(create_cbs);
	create_cbs = 0;
}


int register_qoscb(qos_ctx_t *qos, int types, qos_cb f, void *param )
{
	struct qos_callback *cb;

	LM_DBG("registering qos CB\n");

	if ( types&QOSCB_CREATED ) {
		if (types!=QOSCB_CREATED) {
			LM_CRIT("QOSCB_CREATED type must be register alone!\n");
			return -1;
		}
	} else {
		if (qos==0) {
			LM_CRIT("non-QOSCB_CREATED type "
				"must be register to a qos (qos missing)!\n");
			return -1;
		}
	}
	cb = (struct qos_callback*)shm_malloc(sizeof(struct qos_callback));
	if (cb==0) {
		LM_ERR("no more shm mem\n");
		return -1;
	}

	LM_DBG("cb=%p\n", cb);

	cb->types = types;
	cb->callback = f;
	cb->param = param;

	if ( types&QOSCB_CREATED ) {
		cb->next = create_cbs->first;
		create_cbs->first = cb;
		create_cbs->types |= types;
	} else {
		cb->next = qos->cbs.first;
		qos->cbs.first = cb;
		qos->cbs.types |= types;
		LM_DBG("qos=%p qos->cbs=%p types=%d\n",
			qos, &(qos->cbs), qos->cbs.types);
	}

	return 0;
}


void run_create_cbs(struct qos_ctx_st *qos, struct sip_msg *msg)
{
	struct qos_callback *cb;

	if (create_cbs->first==0)
		return;

	params.msg = msg;
	/* avoid garbage due static structure */
	params.sdp = NULL;
	params.role = 0;
	params.param = NULL;

	for ( cb=create_cbs->first; cb; cb=cb->next)  {
		LM_DBG("qos=%p\n",qos);
		params.param = &cb->param;
		cb->callback( qos, QOSCB_CREATED, &params );
	}
	return;
}


void run_qos_callbacks(int type, struct qos_ctx_st *qos,
			struct qos_sdp_st *sdp, unsigned int role,
			struct sip_msg *msg)
{
	struct qos_callback *cb;

	if (qos == NULL)
		return;

	LM_DBG("qos=%p qos->cbs=%p, qos->cbs.types=%d\n",
		qos, &(qos->cbs),  qos->cbs.types);
	if (qos->cbs.first==0 || ((qos->cbs.types)&type)==0 )
		return;

	params.sdp = sdp;
	params.role = role;
	params.msg = msg;

	LM_DBG("searching in %p\n", qos->cbs.first);
	for ( cb=qos->cbs.first; cb; cb=cb->next)  {
		if ( (cb->types)&type ) {
			LM_DBG("qos=%p, type=%d\n", qos, type);
			params.param = &cb->param;
			cb->callback( qos, type, &params );
		}
	}
	return;
}
