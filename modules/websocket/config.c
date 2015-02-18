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

/*!
 * \file
 * \brief WebSocket :: Configuration
 * \ingroup WebSocket
 */

#include "../../cfg/cfg.h"
#include "ws_frame.h"
#include "config.h"

struct cfg_group_websocket default_ws_cfg =
{
        DEFAULT_KEEPALIVE_TIMEOUT, /* keepalive_timeout */
        1                       /* enabled */
};

void *ws_cfg = &default_ws_cfg;

cfg_def_t ws_cfg_def[] =
{
        /* ws_frame.c */
        { "keepalive_timeout",  CFG_VAR_INT | CFG_ATOMIC,
          0, 0, 0, 0,
          "Time (in seconds) after which to send a keep-alive on idle"
          " WebSocket connections." },

        /* ws_handshake.c */
        { "enabled",            CFG_VAR_INT | CFG_ATOMIC,
          0, 0, 0, 0,
          "Shows whether WebSockets are enabled or not." },

        { 0, 0, 0, 0, 0, 0 }
};
