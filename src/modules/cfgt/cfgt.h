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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *
 */

#ifndef _CFGT_BIND_H
#define _CFGT_BIND_H

#include "../../core/sr_module.h"

/* export not usable from scripts */
#define NO_SCRIPT -1

typedef int (*cfgt_process_route_f)(struct sip_msg *msg, struct action *a);

typedef struct cfgt_api
{
	cfgt_process_route_f cfgt_process_route;
} cfgt_api_t;

/*! cfgt API export bind function */
typedef int (*bind_cfgt_t)(cfgt_api_t *api);

#endif
