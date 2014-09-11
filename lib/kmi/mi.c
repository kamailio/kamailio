/*
 * $Id: mi.c 4565 2008-08-05 14:58:52Z klaus_darilion $
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 * History:
 * ---------
 *  2006-09-08  first version (bogdan)
 */

/*!
 * \file 
 * \brief MI :: Attributes
 * \ingroup mi
 */

/*!
 * \defgroup mi Kamailio Management Interface
 * 
 * The Kamailio management interface (MI) is a plugin architecture with a few different 
 * handlers that gives access to the management interface over various transports.
 *
 * The Kamailio core and modules register commands to the interface at runtime.
 * Look into the various module documentation files for information of these
 * commands.
 *
 */

#include <string.h>

#include "../../dprint.h"
#include "../../sr_module.h"
#include "mi_mem.h"
#include "mi.h"

static struct mi_cmd*  mi_cmds = 0;
static int mi_cmds_no = 0;


static inline int get_mi_id( char *name, int len)
{
	int n;
	int i;

	for( n=0,i=0 ; i<len ; n+=name[i] ,i++ );
	return n;
}


static inline struct mi_cmd* lookup_mi_cmd_id(int id,char *name, int len)
{
	int i;

	for( i=0 ; i<mi_cmds_no ; i++ ) {
		if ( id==mi_cmds[i].id && len==mi_cmds[i].name.len &&
		memcmp(mi_cmds[i].name.s,name,len)==0 )
			return &mi_cmds[i];
	}

	return 0;
}


int register_mi_mod( char *mod_name, mi_export_t *mis)
{
	int ret;
	int i;

	if (mis==0)
		return 0;

	for ( i=0 ; mis[i].name ; i++ ) {
		ret = register_mi_cmd( mis[i].cmd, mis[i].name, mis[i].param,
			mis[i].init_f, mis[i].flags);
		if (ret!=0) {
			LM_ERR("failed to register cmd <%s> for module %s\n",
					mis[i].name,mod_name);
		}
	}
	return 0;
}


static int mi_commands_initialized = 0;


/**
 * Init a process to work properly for MI commands
 * - rank: rank of the process (PROC_XYZ...)
 * - mode: 0 - don't try to init worker for SIP commands
 *         1 - try to init worker for SIP commands
 */
int init_mi_child(int rank, int mode)
{
	int i;

	if(mi_commands_initialized)
		return 0;
	mi_commands_initialized = 1;
	for ( i=0 ; i<mi_cmds_no ; i++ ) {
		if ( mi_cmds[i].init_f && mi_cmds[i].init_f()!=0 ) {
			LM_ERR("failed to init <%.*s>\n",
					mi_cmds[i].name.len,mi_cmds[i].name.s);
			return -1;
		}
	}
	if(mode==1) {
		if(is_sip_worker(rank)) {
			LM_DBG("initalizing proc rpc for sip handling\n");
			if(init_child(PROC_SIPRPC)<0) {
				LM_ERR("failed to init proc rpc for sip handling\n");
				return -1;
			}
		}
	}
	return 0;
}



int register_mi_cmd( mi_cmd_f f, char *name, void *param,
									mi_child_init_f in, unsigned int flags)
{
	struct mi_cmd *cmds;
	int id;
	int len;

	if (f==0 || name==0) {
		LM_ERR("invalid params f=%p, name=%s\n", f, name);
		return -1;
	}

	if (flags&MI_NO_INPUT_FLAG && flags&MI_ASYNC_RPL_FLAG) {
		LM_ERR("invalids flags for <%s> - "
			"async functions must take input\n",name);
	}

	len = strlen(name);
	id = get_mi_id(name,len);

	if (lookup_mi_cmd_id( id, name, len)) {
		LM_ERR("command <%.*s> already registered\n", len, name);
		return -1;
	}

	cmds = (struct mi_cmd*)mi_realloc( mi_cmds,
			(mi_cmds_no+1)*sizeof(struct mi_cmd) );
	if (cmds==0) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}

	mi_cmds = cmds;
	mi_cmds_no++;

	cmds = &cmds[mi_cmds_no-1];

	cmds->f = f;
	cmds->init_f = in;
	cmds->flags = flags;
	cmds->name.s = name;
	cmds->name.len = len;
	cmds->id = id;
	cmds->param = param;

	return 0;
}


struct mi_cmd* lookup_mi_cmd( char *name, int len)
{
	int id;

	id = get_mi_id(name,len);
	return lookup_mi_cmd_id( id, name, len);
}


void get_mi_cmds( struct mi_cmd** cmds, int *size)
{
	*cmds = mi_cmds;
	*size = mi_cmds_no;
}


