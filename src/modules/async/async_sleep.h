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

#include "../../core/parser/msg_parser.h"
#include "../../core/route_struct.h"
#include "../../core/mod_fix.h"

/* clang-format off */
typedef struct async_param {
	int type;
	gparam_t *pinterval;
	union {
		cfg_action_t *paction;
		gparam_t *proute;
	} u;
} async_param_t;

/* clang-format on */

int async_init_timer_list(void);
int async_destroy_timer_list(void);
int async_sleep(sip_msg_t *msg, int seconds, cfg_action_t *act, str *cbname);
void async_timer_exec(unsigned int ticks, void *param);

int async_init_ms_timer_list(void);
int async_destroy_ms_timer_list(void);
int async_ms_sleep(sip_msg_t *msg, int milliseconds, cfg_action_t *act, str *cbname);
void async_mstimer_exec(unsigned int ticks, void *param);

int async_send_task(sip_msg_t *msg, cfg_action_t *act, str *cbname, str *gname);
int async_send_data(sip_msg_t *msg, cfg_action_t *act, str *cbname, str *gname,
		str *sdata);

int pv_get_async(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
int pv_parse_async_name(pv_spec_t *sp, str *in);

sr_kemi_xval_t* ki_async_get_gname(sip_msg_t *msg);
sr_kemi_xval_t* ki_async_get_data(sip_msg_t *msg);

#endif
