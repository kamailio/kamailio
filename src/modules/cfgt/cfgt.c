/*
 * Copyright (C) 2015 Victor Seva (sipwise.com)
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
 *
 */
#include "cfgt_mod.h"
#include "cfgt.h"
#include "cfgt_int.h"

/*!
 * \brief cfgt module API export bind function
 * \param api cfgt API
 * \return 0 on success, -1 on failure
 */
int bind_cfgt(cfgt_api_t *api)
{
	if(!api) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	if(init_flag == 0) {
		LM_ERR("configuration error - trying to bind to cfgt module"
			   " before being initialized\n");
		return -1;
	}

	api->cfgt_process_route = cfgt_process_route;
	return 0;
}
