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

#include "../../tcp_conn.h"
#include "../../lib/kmi/tree.h"
#include "ws_frame.h"
#include "ws_mod.h"

#define FRAME_BUF_SIZE 1024
static char frame_buf[FRAME_BUF_SIZE];

int ws_frame_received(void *data)
{
	int printed;
	str output;
	tcp_event_info_t *tev = (tcp_event_info_t *) data;

	if (tev == NULL || tev->buf == NULL || tev->len <= 0)
	{
		LM_WARN("received bad frame\n");
		return -1;
	}

	output.len = 0;
	output.s = frame_buf;

	for (printed = 0; printed < tev->len && output.len < FRAME_BUF_SIZE - 3;
			printed++)
		output.len += sprintf(output.s + output.len, "%02x ",
				(unsigned char) tev->buf[printed]);
	LM_INFO("Rx: %.*s\n", output.len, output.s);

	return 0;
}

struct mi_root *ws_mi_close(struct mi_root *cmd, void *param)
{
	/* TODO close specified or all connections */
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}

struct mi_root *ws_mi_ping(struct mi_root *cmd, void *param)
{
	/* TODO ping specified connection */
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}
