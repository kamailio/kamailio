/*
 * $Id$
 *
 * Script callbacks -- they add the ability to register callback
 * functions which are always called when script for request
 * processing is entered or left
 *
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


#include <stdlib.h>
#include "script_cb.h"
#include "dprint.h"
#include "error.h"

static struct script_cb *pre_cb=0;
static struct script_cb *post_cb=0;
static unsigned int cb_id=0;

int register_script_cb( cb_function f, callback_t t, void *param )
{
	struct script_cb *new_cb;

	new_cb=malloc(sizeof(struct script_cb));
	if (new_cb==0) {
		LOG(L_ERR, "ERROR: register_script_cb: out of memory\n");
		return E_OUT_OF_MEM;
	}
	new_cb->cbf=f;
	new_cb->id=cb_id++;
	new_cb->param=param;
	/* insert into appropriate list */
	if (t==PRE_SCRIPT_CB) {
		new_cb->next=pre_cb;
		pre_cb=new_cb;
	} else if (t==POST_SCRIPT_CB) {
		new_cb->next=post_cb;
		post_cb=new_cb;
	} else {
		LOG(L_CRIT, "ERROR: register_script_cb: unknown CB type\n");
		return E_BUG;
	}
	/* ok, callback installed */
	return 1;
}

void exec_pre_cb( struct sip_msg *msg)
{
	struct script_cb *i;
	for (i=pre_cb; i; i=i->next) i->cbf(msg, i->param);
}

void exec_post_cb( struct sip_msg *msg)
{
	struct script_cb *i;
	for (i=post_cb; i; i=i->next) i->cbf(msg, i->param);
}

