/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */

#ifndef _WS_MOD_H
#define _WS_MOD_H

#include "../../locking.h"
#include "../../kstats_types.h"
#include "../sl/sl.h"

enum
{
	SUB_PROTOCOL_SIP  = (1 << 0),
	SUB_PROTOCOL_MSRP = (1 << 1)
};

extern sl_api_t ws_slb;
extern gen_lock_t *ws_stats_lock;

extern int ws_ping_interval;	/* time (in seconds) between sending Pings */

#endif /* _WS_MOD_H */
