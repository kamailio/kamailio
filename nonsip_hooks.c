/* 
 * Copyright (C) 2006 iptelorg GmbH
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
 */

/*!
 * \file
 * \brief Kamailio core :: non-sip callbacks, called whenever a message with protocol != SIP/2.0
 * is received (the message must have at least a sip like first line or
 * else they will be dropped before this callbacks are called
 *
 * \ingroup core
 * Module: \ref core
 */

#include "nonsip_hooks.h"
#include "mem/mem.h"

static struct nonsip_hook* nonsip_hooks;
static unsigned int nonsip_max_hooks=MAX_NONSIP_HOOKS;
static int last_hook_idx=0;



int init_nonsip_hooks()
{
	nonsip_hooks=pkg_malloc(nonsip_max_hooks*
									sizeof(struct nonsip_hook));
	if (nonsip_hooks==0){
		goto error;
	}
	memset(nonsip_hooks, 0, nonsip_max_hooks*sizeof(struct nonsip_hook));
	return 0;
error:
	LM_ERR("memory allocation failure\n");
	return -1;
}



void destroy_nonsip_hooks()
{
	int r;
	
	if (nonsip_hooks){
		for (r=0; r<last_hook_idx; r++){
			if (nonsip_hooks[r].destroy)
				nonsip_hooks[r].destroy();
		}
		pkg_free(nonsip_hooks);
		nonsip_hooks=0;
	}
}



/* allocates a new hook
 * returns 0 on success and -1 on error */
int register_nonsip_msg_hook(struct nonsip_hook *h)
{
	struct nonsip_hook* tmp;
	int new_max_hooks;
	
	if (nonsip_max_hooks==0)
		goto error;
	if (last_hook_idx >= nonsip_max_hooks){
		new_max_hooks=2*nonsip_max_hooks;
		tmp=pkg_realloc(nonsip_hooks, 
				new_max_hooks*sizeof(struct nonsip_hook));
		if (tmp==0){
			goto error;
		}
		nonsip_hooks=tmp;
		/* init the new chunk */
		memset(&nonsip_hooks[last_hook_idx+1], 0, 
					(new_max_hooks-nonsip_max_hooks-1)*
						sizeof(struct nonsip_hook));
		nonsip_max_hooks=new_max_hooks;
	}
	nonsip_hooks[last_hook_idx]=*h;
	last_hook_idx++;
	return 0;
error:
	return -1;
}



int nonsip_msg_run_hooks(struct sip_msg* msg)
{
	int r;
	int ret;
	
	ret=NONSIP_MSG_DROP; /* default, if no hook installed, drop */
	for (r=0; r<last_hook_idx; r++){
		ret=nonsip_hooks[r].on_nonsip_req(msg);
		if (ret!=NONSIP_MSG_PASS) break;
	}
	return ret;
}



