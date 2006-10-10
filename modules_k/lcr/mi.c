/*
 * $Id$
 *
 * lcr MI functions
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
 *
 * History:
 * --------
 *  2006-10-05  created (bogdan)
 */


#include "lcr_mod.h"
#include "../../dprint.h"
#include "../../db/db.h"
#include "mi.h"


/*
 * MI function to reload lcr table(s)
 */
struct mi_node*  mi_lcr_reload(struct mi_node* cmd, void* param)
{
	if (reload_gws () == 1) 
		return init_mi_tree(MI_200_OK_S, MI_200_OK_LEN);
	else
		return init_mi_tree("400 Reload of gateways failed",29 );
}


/*
 * MI function to print gws from current gw table
 */
struct mi_node* mi_lcr_dump(struct mi_node* cmd, void* param)
{
	struct mi_node* rpl= NULL;

	rpl= init_mi_tree(MI_200_OK_S, MI_200_OK_LEN);

	if(mi_print_gws(rpl)<0)
	{
		LOG(L_ERR, "lcr:mi_lcr_reload: ERROR while adding node\n");
		free_mi_tree(rpl);
		return 0;
	}

	return rpl;
}

