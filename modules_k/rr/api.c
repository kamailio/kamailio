/*
 * $Id$
 *
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2005-08-02  first version (bogdan)
 */


#include "../../sr_module.h"
#include "loose.h"
#include "record.h"
#include "api.h"
#include "rr_cb.h"


int load_rr( struct rr_binds *rrb )
{
	rrb->add_rr_param      = add_rr_param;
	rrb->check_route_param = check_route_param;
	rrb->is_direction      = is_direction;
	rrb->get_route_param   = get_route_param;
	rrb->register_rrcb     = register_rrcb;

	return 1;
}

