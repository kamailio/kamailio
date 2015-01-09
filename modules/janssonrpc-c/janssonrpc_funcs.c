/**
 * Copyright (C) 2013 Flowroute LLC (flowroute.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "../../mod_fix.h"
#include "../../pvar.h"
#include "../../lvalue.h"
#include "../tm/tm_load.h"

#include "janssonrpc.h"
#include "janssonrpc_request.h"
#include "janssonrpc_io.h"
#include "janssonrpc_funcs.h"


struct tm_binds tmb;

int jsonrpc_request(struct sip_msg* _m,
		char* _conn,
		char* _method,
		char* _params,
		char* _options)
{
	str conn;
	str method;
	str params;
	str options;
	str route;
	param_hooks_t phooks;
	param_t* pit=NULL;
	param_t* freeme=NULL;
	int retry;
	int timeout;
	int retval = -1;

	/* defaults */
	options = null_str;
	route = null_str;
	timeout = JSONRPC_DEFAULT_TIMEOUT;
	retry = JSONRPC_DEFAULT_RETRY;

	if (get_str_fparam(&conn, _m, (fparam_t*)_conn) != 0) {
		ERR("cannot get connection value\n");
		return -1;
	}

	if (get_str_fparam(&method, _m, (fparam_t*)_method) != 0) {
		ERR("cannot get method value\n");
		return -1;
	}

	if (get_str_fparam(&params, _m, (fparam_t*)_params) != 0) {
		ERR("cannot get params value\n");
		return -1;
	}

	if(_options == NULL) {

	} else if (get_str_fparam(&options, _m, (fparam_t*)_options) != 0) {
		ERR("cannot get options value\n");
		return -1;

	} else {
		if(options.len == 0) {
			goto skip_parse;
		}else if (options.len > 0 && options.s[options.len-1] == ';') {
			options.len--;
		}

		if (parse_params(&options, CLASS_ANY, &phooks, &pit)<0) {
			ERR("failed parsing params value\n");
			return -1;
		}

		freeme = pit;

		for (; pit;pit=pit->next)
		{
			if PIT_MATCHES("route") {
				route = pkg_strdup(pit->body);
				CHECK_MALLOC_GOTO(route.s, end);

			} else if PIT_MATCHES("timeout") {
				timeout = atoi(pit->body.s);

			} else if PIT_MATCHES("retry") {
				retry = atoi(pit->body.s);

			} else {
				ERR("Unrecognized option: %.*s\n", STR(pit->name));
				goto end;
			}
		}
	}
skip_parse:

	/* check options */
	if(timeout < 1) {
		ERR("invalid timeout option (%d). Must be > 0.\n", timeout);
		goto end;
	}

	if(retry < -1) {
		ERR("invalid retry option (%d). Must be > -2.\n", retry);
		goto end;
	}

	retval = 0;

	retval = mod_jsonrpc_request(
		_m,                     /* sip_msg */
		conn,                   /* connection group */
		method,                 /* RPC method */
		params,                 /* JSON param */
		route,                  /* result route */
		false,                  /* notify only */
		retry,                  /* retry attempts */
		(unsigned int)timeout   /* request timeout */
		);

end:
	if(freeme) free_params(freeme);
	if(route.s) pkg_free(route.s);
	return retval;
}

int jsonrpc_notification(struct sip_msg* _m,
		char* _conn,
		char* _method,
		char* _params)
{
	str conn;
	str method;
	str params;

	if (get_str_fparam(&conn, _m, (fparam_t*)_conn) != 0) {
		ERR("cannot get connection value\n");
		return -1;
	}

	if (get_str_fparam(&method, _m, (fparam_t*)_method) != 0) {
		ERR("cannot get method value\n");
		return -1;
	}

	if (get_str_fparam(&params, _m, (fparam_t*)_params) != 0) {
		ERR("cannot get params value\n");
		return -1;
	}

	return mod_jsonrpc_request(
		_m,          /* sip_msg */
		conn,        /* connection group */
		method,      /* RPC method */
		params,      /* JSON param */
		null_str,    /* result route */
		true,        /* notify only */
		0,           /* retry attempts */
		0            /* request timeout */
		);
}

int mod_jsonrpc_request(
		struct sip_msg* msg,
		str conn,
		str method,
		str params,
		str route,
		bool notify_only,
		int retry,
		unsigned int timeout
	)
{
	unsigned int hash_index;
	unsigned int label;

	if(retry < -1) {
		ERR("retry can't be less than -1\n");
		return -1;
	}



	jsonrpc_req_cmd_t* req_cmd = create_req_cmd();
	CHECK_MALLOC(req_cmd);

	req_cmd->conn   = shm_strdup(conn);
	CHECK_MALLOC_GOTO(req_cmd->conn.s, error);

	req_cmd->method = shm_strdup(method);
	CHECK_MALLOC_GOTO(req_cmd->conn.s, error);

	if(params.s) {
		req_cmd->params = shm_strdup(params);
		CHECK_MALLOC_GOTO(req_cmd->params.s, error);
	}

	if(route.s) {
		req_cmd->route = shm_strdup(route);
		CHECK_MALLOC_GOTO(req_cmd->route.s, error);
	}

	req_cmd->msg = msg;
	req_cmd->retry = retry;
	req_cmd->notify_only = notify_only;
	req_cmd->timeout = timeout;

	if(notify_only || route.len <= 0) {
		req_cmd->route = null_str;
		if(send_pipe_cmd(CMD_SEND, req_cmd)<0) goto error;
		return 1; /* continue script execution */
	}

	tm_cell_t *t = 0;
	t = tmb.t_gett();
	if (t==NULL || t==T_UNDEFINED)
	{
		if(tmb.t_newtran(msg)<0)
		{
			ERR("cannot create the transaction\n");
			goto error;
		}
		t = tmb.t_gett();
		if (t==NULL || t==T_UNDEFINED)
		{
			ERR("cannot look up the transaction\n");
			goto error;
		}
	}

	if (tmb.t_suspend(msg, &hash_index, &label) < 0) {
		ERR("t_suspend() failed\n");
		goto error;
	}
	req_cmd->t_hash = hash_index;
	req_cmd->t_label = label;

	if(send_pipe_cmd(CMD_SEND, req_cmd)<0) goto error;

	return 0;

error:
	free_req_cmd(req_cmd);
	ERR("failed to send request to io process\n");
	return -1;
}

