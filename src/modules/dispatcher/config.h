/*
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
 * \brief Dispatcher :: Configuration
 * \ingroup dispatcher
 */


#ifndef _DISPATCHER_CONFIG_H
#define _DISPATCHER_CONFIG_H

#include "../../qvalue.h"

#include "../../cfg/cfg.h"
#include "../../str.h"

struct cfg_group_dispatcher {
	int probing_threshold;
	int inactive_threshold;
	str ds_ping_reply_codes_str;
};

extern struct cfg_group_dispatcher	default_dispatcher_cfg;
extern void	*dispatcher_cfg;
extern cfg_def_t	dispatcher_cfg_def[];

extern void ds_ping_reply_codes_update(str*, str*);

#endif
