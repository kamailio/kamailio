/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "stdlib.h"
#include "../../dprint.h"
#include "../../error.h"
#include "t_hooks.h"

/* strange things happen if callback_array is static on openbsd */
struct tm_callback_s* callback_array[ TMCB_END ] = { 0, 0 } ;
static int callback_id=0;

/* register a callback function 'f' of type 'cbt'; will be called
   back whenever the event 'cbt' occurs in transaction module
*/
int register_tmcb( tmcb_type cbt, transaction_cb f, void *param )
{
	struct tm_callback_s *cbs;

	if (cbt<0 || cbt>=TMCB_END ) {
		LOG(L_ERR, "ERROR: register_tmcb: invalid callback type: %d\n",
			cbt );
		return E_BUG;
	}

	if (!(cbs=malloc( sizeof( struct tm_callback_s)))) {
		LOG(L_ERR, "ERROR: register_tmcb: out of mem\n");
		return E_OUT_OF_MEM;
	}

	callback_id++;
	cbs->id=callback_id;
	cbs->callback=f;
	cbs->next=callback_array[ cbt ];
	cbs->param=param;
	callback_array[ cbt ]=cbs;

	return callback_id;
}

void callback_event( tmcb_type cbt , struct cell *trans,
	struct sip_msg *msg, int code )
{
	struct tm_callback_s *cbs;

	for (cbs=callback_array[ cbt ]; cbs; cbs=cbs->next)  {
		DBG("DBG: callback type %d, id %d entered\n", cbt, cbs->id );
		cbs->callback( trans, msg, code, cbs->param );
	}
}
