/*
 * $Id$
 *
 * Copyright (C) 2012 Crocodile RCS Ltd
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

#ifndef _WS_MOD_H
#define _WS_MOD_H

#include "../sl/sl.h"

extern sl_api_t ws_slb;
extern int ws_ping_interval;	/* time (in seconds) after which a Ping will be
				   sent on an idle connection */
extern int ws_ping_timeout;	/* time (in seconds) to wait for a Pong in
				   response to a Ping before closing a
				   connection */
#endif /* _WS_MOD_H */
