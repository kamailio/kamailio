/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * History:
 * ---------
 *  2006-03-29  first version (bogdan)
 */

/*!
 * \file
 * \brief SL :: callbacks
 * \ingroup sl
 * - Module: \ref sl
 */

#include "../../mem/mem.h"
#include "sl_cb.h"

/*! callback list head */
struct sl_callback* slcb_hl = 0;


/*! cleanup callback list */
void destroy_slcb_lists(void)
{
	struct sl_callback *cbp, *cbp_tmp;

	for( cbp=slcb_hl; cbp ; ) {
		cbp_tmp = cbp;
		cbp = cbp->next;
		pkg_free( cbp_tmp );
	}
}


/*! register a SL callback */
int register_slcb(unsigned int types, sl_cb_t f, void *param )
{
	struct sl_callback *cbp;

	/* build a new callback structure */
	if (!(cbp=pkg_malloc( sizeof( struct sl_callback)))) {
		LM_ERR("out of pkg. mem\n");
		return -1;
	}

	/* fill it up */
	cbp->types = types;
	cbp->callback = f;
	cbp->param = param;
	/* link it at the beginning of the list */
	cbp->next = slcb_hl;
	slcb_hl = cbp;
	/* set next id */
	if (cbp->next)
		cbp->id = cbp->next->id+1;
	else
		cbp->id = 0;

	return 0;
}


/*! run SL callbacks */
void run_sl_callbacks( unsigned int types, struct sip_msg *req, str *buffer,
							int code, str *reason, union sockaddr_union *to )
{
	static struct sl_cb_param cb_params;
	struct sl_callback *cbp;

	cb_params.buffer = buffer;
	cb_params.code = code;
	cb_params.reason = reason;
	cb_params.dst = to;

	for ( cbp=slcb_hl ; cbp ; cbp=cbp->next ) {
		if (types&cbp->types) {
			cb_params.param = cbp->param;
			LM_DBG("callback id %d entered\n", cbp->id );
				cbp->callback( types&cbp->types, req, &cb_params);
		}
	}
}
