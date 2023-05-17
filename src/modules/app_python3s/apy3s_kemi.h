/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __APY_KEMI_H__
#define __APY_KEMI_H__

#include <Python.h>
#include "../../core/parser/msg_parser.h"

int sr_apy3s_init_ksr(void);
void sr_apy_destroy_ksr(void);
int sr_kemi_config_engine_python(
		sip_msg_t *msg, int rtype, str *rname, str *rparam);

int apy3s_exec_func(sip_msg_t *_msg, char *fname, char *fparam, int emode);

PyObject *sr_apy_kemi_exec_func(PyObject *self, PyObject *args, int idx);

int apy_sr_init_mod(void);
int app_python3s_init_rpc(void);
int apy_load_script(void);
int apy_init_script(int rank);

typedef struct sr_apy_env
{
	sip_msg_t *msg;
} sr_apy_env_t;

sr_apy_env_t *sr_apy_env_get();

#endif
