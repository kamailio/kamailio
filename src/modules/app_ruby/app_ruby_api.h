/**
 * Copyright (C) 2018 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _APP_RUBY_API_H_
#define _APP_RUBY_API_H_

#include <ruby.h>

#include "../../core/parser/msg_parser.h"

typedef VALUE (*app_ruby_function)(int argc, VALUE* argv, VALUE self);

typedef struct _ksr_ruby_context {
	int ctxid;
} ksr_ruby_context_t;

typedef struct _ksr_ruby_export {
	char *mname;
	char *fname;
	app_ruby_function func;
} ksr_ruby_export_t;

int ruby_sr_init_mod(void);
int ruby_sr_init_child(void);
void ruby_sr_destroy(void);

int ruby_sr_initialized(void);

VALUE sr_kemi_ruby_exec_func(ksr_ruby_context_t *R, int eidx, int argc,
		VALUE* argv, VALUE self);

int app_ruby_run_ex(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3, int emode);
int app_ruby_run(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3);
int app_ruby_runstring(sip_msg_t *msg, char *script);
int app_ruby_dostring(sip_msg_t *msg, char *script);
int app_ruby_dofile(sip_msg_t *msg, char *script);

int app_ruby_init_rpc(void);
#endif
