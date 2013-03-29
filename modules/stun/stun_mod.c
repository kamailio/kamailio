/*
 * $Id$
 *
 * Copyright (C) 2013 Crocodile RCS Ltd
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "../../dprint.h"
#include "../../events.h"
#include "../../ip_addr.h"
#include "../../sr_module.h"
#include "kam_stun.h"

MODULE_VERSION

static int mod_init(void);
static int stun_msg_receive(void *data);

struct module_exports exports= 
{
	"stun",
	DEFAULT_DLFLAGS,	/* dlopen flags */
	0,			/* Exported functions */
	0,			/* Exported parameters */
	0,			/* exported statistics */
	0,			/* exported MI functions */
	0,			/* exported pseudo-variables */
	0,			/* extra processes */
	mod_init,		/* module initialization function */
	0,			/* response function */
	0,			/* destroy function */
	0			/* per-child initialization function */
};

static int mod_init(void)
{
	if (sr_event_register_cb(SREV_STUN_IN, stun_msg_receive) != 0)
	{
		LM_ERR("registering STUN receive call-back\n");
		return -1;
	}

	return 0;
}

int stun_msg_receive(void *data)
{
	stun_event_info_t *sev = (stun_event_info_t *) data;
	return process_stun_msg(sev->buf, sev->len, sev->rcv);
}
