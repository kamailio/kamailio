/**
 * $Id$
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

		       
#ifndef _DEBUGGER_API_H_
#define _DEBUGGER_API_H_

#include "../../route_struct.h"

int dbg_add_breakpoint(struct action *a, int bpon);
int dbg_init_bp_list(void);
int dbg_init_pid_list(void);
int dbg_init_mypid(void);
int dbg_init_rpc(void);

int dbg_init_mod_levels(int _dbg_mod_level, int _dbg_mod_hash_size);
int dbg_set_mod_debug_level(char *mname, int mnlen, int *mlevel);
void dbg_enable_mod_levels(void);

#endif

