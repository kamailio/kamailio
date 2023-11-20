/**
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _APP_JSDT_API_H_
#define _APP_JSDT_API_H_

#include "../../core/parser/msg_parser.h"
#include "duktape.h"

int jsdt_sr_init_mod(void);
int jsdt_sr_init_child(int rank);
void jsdt_sr_destroy(void);

int jsdt_sr_initialized(void);

int sr_kemi_jsdt_exec_func(duk_context *J, int eidx);

int app_jsdt_run_ex(
		sip_msg_t *msg, char *func, char *p1, char *p2, char *p3, int emode);
int app_jsdt_run(sip_msg_t *msg, char *func, char *p1, char *p2, char *p3);
int app_jsdt_runstring(sip_msg_t *msg, char *script);
int app_jsdt_dostring(sip_msg_t *msg, char *script);
int app_jsdt_dofile(sip_msg_t *msg, char *script);

int app_jsdt_init_rpc(void);

duk_ret_t cb_resolve_module(duk_context *JJ);
duk_ret_t cb_load_module(duk_context *JJ);
#endif
