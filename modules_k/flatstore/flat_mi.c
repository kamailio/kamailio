/* 
 * $Id$ 
 *
 * Flatstore module MI interface
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "flatstore_mod.h"
#include "flat_mi.h"


struct mi_node*  mi_flat_rotate_cmd(struct mi_node* cmd, void* param)
{
	struct mi_node *rpl;

	rpl = init_mi_tree(MI_200_OK_S, MI_200_OK_LEN);
	if(rpl == NULL)
		return rpl;

	*flat_rotate = time(0);

	return rpl;
}
