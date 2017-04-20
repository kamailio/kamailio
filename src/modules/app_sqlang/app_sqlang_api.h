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

#include <squirrel.h>

#include "../../core/parser/msg_parser.h"

int sqlang_sr_init_mod(void);
int sqlang_sr_init_child(void);
void sqlang_sr_destroy(void);

int sqlang_sr_initialized(void);

int sr_kemi_sqlang_exec_func(HSQUIRRELVM J, int eidx);

int app_sqlang_run_ex(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3, int emode);
int app_sqlang_run(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3);
int app_sqlang_runstring(sip_msg_t *msg, char *script);
int app_sqlang_dostring(sip_msg_t *msg, char *script);
int app_sqlang_dofile(sip_msg_t *msg, char *script);

int app_sqlang_init_rpc(void);
#endif
