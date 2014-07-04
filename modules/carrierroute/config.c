/*
 * $Id$
 * This file is part of SIP-router, a free SIP server.
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
 * History
 * -------
 *  2008-02-05	adapting tm module for the configuration framework (Miklos)
 */

/*!
 * \file 
 * \brief Siputils :: Configuration
 * \ingroup Siputils
 */


#include "../../cfg/cfg.h"
#include "../../parser/msg_parser.h" /* method types */

#include "config.h"

struct cfg_group_carrierroute	default_carrierroute_cfg = {
		0, 	/* use_domain */
		1, 	/* fallback_default */
		2000	/* fetch_rows*/ 
	};

void	*carrierroute_cfg = &default_carrierroute_cfg;

cfg_def_t	carrierroute_cfg_def[] = {
	{"use_domain",		CFG_VAR_INT ,	0, 1, 0, 0,
		"When using tree lookup per user, this parameter specifies whether to use the domain part for user matching or not." },
	{"fallback_default",	CFG_VAR_INT ,	0, 1, 0, 0,
		"If the user has a non-existing tree set and fallback_default is set to 1, the default tree is used. Else error is returned" },
	{"fetch_rows",	CFG_VAR_INT ,	0, 0, 0, 0,
		"The number of the rows to be fetched at once from database when loading the routing data."},
	{0, 0, 0, 0, 0, 0}
};

