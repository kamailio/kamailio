/*
 * Copyright (C) 2011 Flowroute LLC (flowroute.com)
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

#include "../../mod_fix.h"
#include "../../pvar.h"
#include "../../lvalue.h"
#include "../tm/tm_load.h"

#include "jsonrpc_request.h"
#include "jsonrpc_io.h"


struct tm_binds tmb;
static char *shm_strdup(str *src);

int memory_error() {
	LM_ERR("Out of memory!");
	return -1;
}

int jsonrpc_request(struct sip_msg* _m, char* _method, char* _params, char* _cb_route, char* _err_route, char* _cb_pv)
{
  str method;
  str params;
  str cb_route;
  str err_route;
	

	if (fixup_get_svalue(_m, (gparam_p)_method, &method) != 0) {
		LM_ERR("cannot get method value\n");
		return -1;
	}
	if (fixup_get_svalue(_m, (gparam_p)_params, &params) != 0) {
		LM_ERR("cannot get params value\n");
		return -1;
	}
	if (fixup_get_svalue(_m, (gparam_p)_cb_route, &cb_route) != 0) {
		LM_ERR("cannot get cb_route value\n");
		return -1;
	}

	if (fixup_get_svalue(_m, (gparam_p)_err_route, &err_route) != 0) {
		LM_ERR("cannot get err_route value\n");
		return -1;
	}

	tm_cell_t *t = 0;
	t = tmb.t_gett();
	if (t==NULL || t==T_UNDEFINED)
	{
		if(tmb.t_newtran(_m)<0)
		{
			LM_ERR("cannot create the transaction\n");
			return -1;
		}
		t = tmb.t_gett();
		if (t==NULL || t==T_UNDEFINED)
		{
			LM_ERR("cannot look up the transaction\n");
			return -1;
		}
	}

	unsigned int hash_index;
	unsigned int label;

	if (tmb.t_suspend(_m, &hash_index, &label) < 0) {
		LM_ERR("t_suspend() failed\n");
		return -1;
	}

	struct jsonrpc_pipe_cmd *cmd;
	if (!(cmd = (struct jsonrpc_pipe_cmd *) shm_malloc(sizeof(struct jsonrpc_pipe_cmd))))
		return memory_error();

	memset(cmd, 0, sizeof(struct jsonrpc_pipe_cmd));

	pv_spec_t *cb_pv = (pv_spec_t*)shm_malloc(sizeof(pv_spec_t));
	if (!cb_pv)
		return memory_error();

	cb_pv = memcpy(cb_pv, (pv_spec_t *)_cb_pv, sizeof(pv_spec_t));

	cmd->method = shm_strdup(&method);
	cmd->params = shm_strdup(&params);
	cmd->cb_route = shm_strdup(&cb_route);
	cmd->err_route = shm_strdup(&err_route);
	cmd->cb_pv = cb_pv;
	cmd->msg = _m;
	cmd->t_hash = hash_index;
	cmd->t_label = label;
	
	if (write(cmd_pipe, &cmd, sizeof(cmd)) != sizeof(cmd)) {
		LM_ERR("failed to write to io pipe: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

int jsonrpc_notification(struct sip_msg* _m, char* _method, char* _params)
{
	str method;
	str params;

	if (fixup_get_svalue(_m, (gparam_p)_method, &method) != 0) {
		LM_ERR("cannot get method value\n");
		return -1;
	}
	if (fixup_get_svalue(_m, (gparam_p)_params, &params) != 0) {
		LM_ERR("cannot get params value\n");
		return -1;
	}

	struct jsonrpc_pipe_cmd *cmd;
	if (!(cmd = (struct jsonrpc_pipe_cmd *) shm_malloc(sizeof(struct jsonrpc_pipe_cmd))))
		return memory_error();

	memset(cmd, 0, sizeof(struct jsonrpc_pipe_cmd));

	cmd->method = shm_strdup(&method);
	cmd->params = shm_strdup(&params);
	cmd->notify_only = 1;

	if (write(cmd_pipe, &cmd, sizeof(cmd)) != sizeof(cmd)) {
		LM_ERR("failed to write to io pipe: %s\n", strerror(errno));
		return -1;
	}

	return 1;
}

static char *shm_strdup(str *src)
{
	char *res;

	if (!src || !src->s)
		return NULL;
	if (!(res = (char *) shm_malloc(src->len + 1)))
		return NULL;
	strncpy(res, src->s, src->len);
	res[src->len] = 0;
	return res;
}
