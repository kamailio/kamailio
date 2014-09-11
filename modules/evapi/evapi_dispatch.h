/**
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _EVAPI_DISPATCH_
#define _EVAPI_DISPATCH_

#include "../../pvar.h"

int evapi_init_notify_sockets(void);

void evapi_close_notify_sockets_child(void);

void evapi_close_notify_sockets_parent(void);

int evapi_run_dispatcher(char *laddr, int lport);

int evapi_run_worker(int prank);

int evapi_relay(str *evdata);

void evapi_init_environment(int dformat);

int pv_parse_evapi_name(pv_spec_t *sp, str *in);
int pv_get_evapi(sip_msg_t *msg,  pv_param_t *param, pv_value_t *res);
int pv_set_evapi(sip_msg_t *msg, pv_param_t *param, int op,
		pv_value_t *val);

/* set evapi env to shortcut of hdr date - not used in faked msg */
#define evapi_set_msg_env(_msg, _evenv) do { _msg->date=(hdr_field_t*)_evenv; } while(0)
#define evapi_get_msg_env(_msg) ((evapi_env_t*)_msg->date)

int evapi_cfg_close(sip_msg_t *msg);

#endif
