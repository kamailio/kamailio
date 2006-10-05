/*
 * $Id$
 *
 * Domain MI functions
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


#include "../../dprint.h"
#include "../../db/db.h"
#include "../../mi/mi.h"
#include "domain_mod.h"
#include "domain.h"
#include "hash.h"
#include "mi.h"


/*
 * MI function to reload domain table
 */
static struct mi_node* mi_domain_reload(struct mi_node *cmd, void *param)
{
	if (reload_domain_table () == 1) {
		return init_mi_tree(MI_200_OK_S, MI_200_OK_LEN);
	} else {
		return init_mi_tree("400 Domain table reload failed", 30);
	}
}


/*
 * MI function to print domains from current hash table
 */
static struct mi_node* mi_domain_dump(struct mi_node *cmd, void *param)
{
	struct mi_node* rpl;

	rpl = init_mi_tree(MI_200_OK_S, MI_200_OK_LEN);
	if (rpl==NULL)
		return 0;

	if(hash_table_mi_print(*hash_table, rpl)< 0)
	{
		LOG(L_ERR,"domain:mi_domain_dump: Error while adding node\n");
		free_mi_tree(rpl);
		return 0;
	}

	return rpl;
}


/*
 * Register domain MI functions
 */
int init_domain_mi( void )
{
	if (register_mi_cmd(mi_domain_reload, MI_DOMAIN_RELOAD, 0) < 0) {
		LOG(L_CRIT, "Cannot register %s\n",MI_DOMAIN_RELOAD);
		return -1;
	}

	if (register_mi_cmd(mi_domain_dump, MI_DOMAIN_DUMP, 0) < 0) {
		LOG(L_CRIT, "Cannot register %s\n",MI_DOMAIN_DUMP);
		return -1;
	}

	return 1;
}
