/*
 * $Id$
 *
 * Copyright (C) 2012-2013 Crocodile RCS Ltd
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

/*!
 * \file
 * \brief WebSocket :: Configuration Framework support
 * \ingroup WebSocket
 */

#ifndef _WS_CONFIG_H
#define _WS_CONFIG_H

#include "../../qvalue.h"
#include "../../str.h"
#include "../../cfg/cfg.h"

struct cfg_group_websocket
{
	int keepalive_timeout;
	int enabled;
};
extern struct cfg_group_websocket default_ws_cfg;
extern void *ws_cfg;
extern cfg_def_t ws_cfg_def[];

#endif /* _WS_CONFIG_H */
