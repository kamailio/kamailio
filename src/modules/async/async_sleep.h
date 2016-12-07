/**
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _ASYNC_SLEEP_H_
#define _ASYNC_SLEEP_H_

#include "../../parser/msg_parser.h"
#include "../../route_struct.h"
#include "../../mod_fix.h"

typedef struct async_param {
	int type;
	gparam_t *pinterval;
	union {
		cfg_action_t *paction;
		gparam_t *proute;
	} u;
} async_param_t;

int async_init_timer_list(void);

int async_destroy_timer_list(void);

int async_sleep(sip_msg_t* msg, int seconds, cfg_action_t *act);

void async_timer_exec(unsigned int ticks, void *param);

int async_send_task(sip_msg_t* msg, cfg_action_t *act);

#endif
