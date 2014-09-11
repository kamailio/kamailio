/*
 * $Id: mi.h 5003 2008-09-26 11:01:51Z henningw $
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
 *  2010-08-18  use mi types from ../../mi/mi_types.h (andrei)
 */

/*!
 * \file 
 * \brief MI :: Management
 * \ingroup mi
 */


#ifndef _MI_MI_H_
#define _MI_MI_H_

#include "../../str.h"
#include "../../mi/mi_types.h"
#include "tree.h"

#define MI_ASYNC_RPL_FLAG   (1<<0)
#define MI_NO_INPUT_FLAG    (1<<1)

#define MI_ROOT_ASYNC_RPL   ((struct mi_root*)-1)



struct mi_handler {
	mi_handler_f *handler_f;
	void * param;
};


struct mi_cmd {
	int id;
	str name;
	mi_child_init_f *init_f;
	mi_cmd_f *f;
	unsigned int flags;
	void *param;
};


int register_mi_cmd( mi_cmd_f f, char *name, void *param,
		mi_child_init_f in, unsigned int flags);

int register_mi_mod( char *mod_name, mi_export_t *mis);

int init_mi_child(int rank, int mode);

struct mi_cmd* lookup_mi_cmd( char *name, int len);

static inline struct mi_root* run_mi_cmd(struct mi_cmd *cmd, struct mi_root *t)
{
	return cmd->f( t, cmd->param);
}

void get_mi_cmds( struct mi_cmd** cmds, int *size);

int init_mi_core(void);

#endif

