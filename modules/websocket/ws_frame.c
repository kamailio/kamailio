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

#include "../../lib/kmi/tree.h"
#include "ws_frame.h"
#include "ws_mod.h"

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
