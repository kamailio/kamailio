/*
 * $Id$
 *
 * Copyright (C) 2008 iptelorg GmbH
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History
 * -------
 *  2008-02-05	adapting tm module for the configuration framework (Miklos)
 */

/*!
 * \file 
 * \brief TM :: Configuration
 * \ingroup tm
 */


#include "../../cfg/cfg.h"
#include "../../parser/msg_parser.h" /* method types */

#include "config.h"

struct cfg_group_ratelimit	default_ratelimit_cfg = {
		DEFAULT_REPLY_CODE,
		DEFAULT_REPLY_REASON
};

void	*ratelimit_cfg = &default_ratelimit_cfg;

cfg_def_t	ratelimit_cfg_def[] = {
	{"reply_code",	CFG_VAR_INT | CFG_ATOMIC,	400, 699, 0, 0,
		"The code of the reply sent by Kamailio while limiting." },
	{"reply_reason",	CFG_VAR_STRING,	0, 0, 0, 0,
		"The reason of the reply sent by Kamailio while limiting."},
	{0, 0, 0, 0, 0, 0}
};
