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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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

int dbg_init_mod_levels(int _dbg_mod_hash_size);
int dbg_set_mod_debug_level(char *mname, int mnlen, int *mlevel);
void dbg_enable_mod_levels(void);

int dbg_init_pvcache(void);
void dbg_enable_log_assign(void);

/*!
 * \brief Callback function that checks if reset_msgid is set
 *  and modifies msg->id if necessary.
 * \param msg SIP message
 * \param flags unused
 * \param bar unused
 * \return 1 on success, -1 on failure
 */
int dbg_msgid_filter(struct sip_msg *msg, unsigned int flags, void *bar);

#define DBG_DP_NULL			1
#define DBG_DP_AVP			2
#define DBG_DP_SCRIPTVAR	4
#define DBG_DP_XAVP			8
#define DBG_DP_OTHER		16
#define DBG_DP_ALL			31
int dbg_dump_json(struct sip_msg* msg, unsigned int mask, int level);
#endif

