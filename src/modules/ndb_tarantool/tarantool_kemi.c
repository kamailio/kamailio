/*
 * Copyright (C) 2026 Kamailio Project
 *
 * tarantool_kemi.c - Implementation of KEMI Lua/Python bindings for Tarantool 3.x
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tarantool_client.h"
#include "tarantool_kemi.h"

extern tnt_pool_t tnt_global_pool;

int sr_kemi_tarantool_call(
		void *msg, str_t *proc_name, str_t *params_json, str_t *res_dst)
{
	(void)msg;
	if(!proc_name || !proc_name->s || proc_name->len <= 0) {
		return -1;
	}

	char name_buf[128];
	int nlen = proc_name->len < (int)sizeof(name_buf) - 1
					   ? proc_name->len
					   : (int)sizeof(name_buf) - 1;
	memcpy(name_buf, proc_name->s, nlen);
	name_buf[nlen] = '\0';

	char params_buf[1024];
	if(params_json && params_json->s && params_json->len > 0) {
		int plen = params_json->len < (int)sizeof(params_buf) - 1
						   ? params_json->len
						   : (int)sizeof(params_buf) - 1;
		memcpy(params_buf, params_json->s, plen);
		params_buf[plen] = '\0';
	} else {
		strcpy(params_buf, "[]");
	}

	tnt_conn_t *conn = tnt_pool_get_conn(&tnt_global_pool);
	if(!conn)
		return -1;

	char res_buf[2048];
	int rc = tnt_call_procedure(
			conn, name_buf, params_buf, res_buf, sizeof(res_buf));
	tnt_pool_put_conn(&tnt_global_pool, conn);

	if(rc == 0 && res_dst) {
		/* Copy result into destination string */
		res_dst->s = strdup(res_buf);
		res_dst->len = (int)strlen(res_buf);
	}

	return rc == 0 ? 1 : -1;
}

int sr_kemi_tarantool_eval(
		void *msg, str_t *lua_code, str_t *params_json, str_t *res_dst)
{
	(void)msg;
	if(!lua_code || !lua_code->s || lua_code->len <= 0) {
		return -1;
	}

	tnt_conn_t *conn = tnt_pool_get_conn(&tnt_global_pool);
	if(!conn)
		return -1;

	char res_buf[2048];
	int rc = tnt_eval_code(conn, lua_code->s,
			params_json ? params_json->s : "[]", res_buf, sizeof(res_buf));
	tnt_pool_put_conn(&tnt_global_pool, conn);

	if(rc == 0 && res_dst) {
		/* Copy result into destination string */
		res_dst->s = strdup(res_buf);
		res_dst->len = (int)strlen(res_buf);
	}

	return rc == 0 ? 1 : -1;
}
