/*
 * This file is part of Kamailio, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*!
 * \file 
 * \brief Dispatcher :: Configuration
 * \ingroup Dispatcher
 */


#include "../../cfg/cfg.h"
#include "../../parser/msg_parser.h" /* method types */

#include "config.h"

struct cfg_group_dispatcher	default_dispatcher_cfg = {
		1,	/* Probing threshold */	
		1,      /* Inactive threshold */
		{0,0}	/* reply codes */
	    };

void	*dispatcher_cfg = &default_dispatcher_cfg;

cfg_def_t	dispatcher_cfg_def[] = {
	{"probing_threshold",		CFG_VAR_INT | CFG_ATOMIC,
		0, 0, 0, 0,
		"Number of failed requests, before a destination is set to probing."},
	{"inactive_threshold",           CFG_VAR_INT | CFG_ATOMIC,
		0, 0, 0, 0,
        "Number of successful requests, before a destination is set to active."},
	{"ping_reply_codes",		CFG_VAR_STR | CFG_CB_ONLY_ONCE
		,			0, 0, 0, ds_ping_reply_codes_update,
		"Additional, valid reply codes for the OPTIONS Pinger. Default is \"\""},
	{0, 0, 0, 0, 0, 0}
};
