/*
 * Copyright (C) 2005 Voice Sistem SRL
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*!
 * \file
 * \brief Route & Record-Route module, callback API
 * \ingroup rr
 */

#include "../../mem/mem.h"
#include "rr_cb.h"


/*! global callback list */
struct rr_callback* rrcb_hl = 0;  /* head list */


/*!
 * \brief destroy global callback list, frees memory
 */
void destroy_rrcb_lists(void)
{
	struct rr_callback *cbp, *cbp_tmp;

	for( cbp=rrcb_hl; cbp ; ) {
		cbp_tmp = cbp;
		cbp = cbp->next;
		pkg_free( cbp_tmp );
	}
}


/*!
 * \brief register a RR callback, allocates new private memory for it
 * \param f callback register function
 * \param param callback parameter
 * \return 0 on success, -1 on failure (out of memory)
 */
int register_rrcb( rr_cb_t f, void *param )
{
	struct rr_callback *cbp;

	/* build a new callback structure */
	if (!(cbp=pkg_malloc( sizeof( struct rr_callback)))) {
		LM_ERR("no more pkg mem\n");
		return -1;
	}

	/* fill it up */
	cbp->callback = f;
	cbp->param = param;
	/* link it at the beginning of the list */
	cbp->next = rrcb_hl;
	rrcb_hl = cbp;
	/* set next id */
	if (cbp->next)
		cbp->id = cbp->next->id+1;
	else
		cbp->id = 0;

	return 0;
}


/*!
 * \brief run RR transaction callbacks
 * \param req SIP request
 * \param rr_param callback list
 */
void run_rr_callbacks( struct sip_msg *req, str *rr_param )
{
	str l_param;
	struct rr_callback *cbp;

	for ( cbp=rrcb_hl ; cbp ; cbp=cbp->next ) {
		l_param = *rr_param;
		LM_DBG("callback id %d entered with <%.*s>\n",
			cbp->id , l_param.len,l_param.s);
		cbp->callback( req, &l_param, cbp->param );
	}
}
