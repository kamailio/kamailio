/**
 * Copyright (C) 2015 Bicom Systems Ltd, (bicomsystems.com)
 *
 * Author: Seudin Kasumovic (seudin.kasumovic@gmail.com)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <string.h>

#include <ei.h>

#include "handle_emsg.h"
#include "handle_rpc.h"
#include "erl_helpers.h"
#include "cnode.h"
#include "pv_xbuff.h"
#include "mod_erlang.h"

#include "../../dprint.h"
#include "../../sr_module.h"
#include "../../cfg/cfg_struct.h"
#include "../../lib/kcore/faked_msg.h"

int handle_msg_req_tuple(cnode_handler_t *phandler, erlang_msg * msg);
int handle_req_ref_tuple(cnode_handler_t *phandler, erlang_msg * msg);
int handle_rpc_response(cnode_handler_t *phandler, erlang_msg * msg, int arity);
int handle_rex_call(cnode_handler_t *phandler,erlang_ref_ex_t *ref, erlang_pid *pid);
int handle_net_kernel(cnode_handler_t *phandler, erlang_msg * msg);
void encode_error_msg(ei_x_buff *response, erlang_ref_ex_t *ref, const char *type, const char *msg );

int handle_reg_send(cnode_handler_t *phandler, erlang_msg * msg)
{
	int rt, backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;
	sip_msg_t tmsg;
	char *route;
	size_t sz;
	ei_x_buff *request = &phandler->request;
	sr_xavp_t *xreq = NULL;
	sr_xavp_t *xpid = NULL;
	str msg_name = str_init("msg");
	str pid_name = str_init("pid");
	sr_xval_t val;
	sr_data_t *data = NULL;

	sz = sizeof("erlang")+strlen(msg->toname)+2;
	route = (char*)pkg_malloc(sz);
	if (!route) {
		LM_ERR("not enough memory");
		return -1;
	}

	snprintf(route,sz,"erlang:%s",msg->toname);

	rt = route_get(&event_rt, route);
	if (rt < 0 || event_rt.rlist[rt] == NULL) {
		LM_WARN("ERL_REG_SEND message to unknown process %s\n", route);
		pkg_free(route);
		PRINT_DBG_REG_SEND(phandler->conn.nodename, msg->from, phandler->ec.thisnodename, msg->toname,request);
		return 0;
	}

	LM_DBG("executing registered process %s\n", route);

	fmsg = faked_msg_next();
	memcpy(&tmsg, fmsg, sizeof(sip_msg_t));
	fmsg = &tmsg;

	if ((xreq = pv_xbuff_get_xbuff(&msg_name))) {
		LM_DBG("free previous $xbuff(msg) value\n");
		xavp_destroy_list(&xreq->val.v.xavp);
	} else {
		xreq = xbuff_new(&msg_name);
	}

	if (!xreq) {
		LM_ERR("failed to create $xbuff(msg) variable\n");
		goto err;
	}

	/* decode request into $xbuff(msg) */
	xreq->val.type = SR_XTYPE_XAVP;

	/* XAVP <- ei_x_buff */
	if (erl_api.xbuff2xavp(&xreq->val.v.xavp,request)){
		LM_ERR("failed to decode message\n");
		goto err;
	}

	if ((xpid = pv_xbuff_get_xbuff(&pid_name))) {
		LM_DBG("free previous $xbuff(pid) value\n");
		xavp_destroy_list(&xpid->val.v.xavp);
	} else {
		xpid = xbuff_new(&pid_name);
	}

	if (!xpid) {
		LM_ERR("failed to create $xbuff(pid) variable\n");
		goto err;
	}

	/* put erl_pid into $xbuff(pid) */
	data = (sr_data_t*)shm_malloc(sizeof(sr_data_t)+sizeof(erlang_pid));
	if (!data) {
		LM_ERR("not enough shared memory\n");
		goto err;
	}

	memset((void*)data,0,sizeof(sr_data_t)+sizeof(erlang_pid));

	data->p = (void*)data+sizeof(sr_data_t);
	data->pfree = xbuff_data_free;

	memcpy(data->p,(void*)&msg->from,sizeof(erlang_pid));

	val.type = SR_XTYPE_DATA;
	val.v.data = data;

	xpid->val.v.xavp = xavp_new_value(&pid_name,&val);
	if (!xpid->val.v.xavp) {
		LM_ERR("failed to create xavp!\n");
		goto err;
	}
	xpid->val.type = SR_XTYPE_XAVP;

	/* registered process reply to from */
	cnode_reply_to_pid = &msg->from;

	backup_rt = get_route_type();
	set_route_type(EVENT_ROUTE);
	init_run_actions_ctx(&ctx);
	run_top_route(event_rt.rlist[rt], fmsg, &ctx);
	set_route_type(backup_rt);

	pkg_free(route);
	free_xbuff_fmt_buff();
	xavp_destroy_list(xavp_get_crt_list());
	return 0;

err:
	shm_free(data);
	pkg_free(route);
	free_xbuff_fmt_buff();
	xavp_destroy_list(xavp_get_crt_list());
	return -1;
}

/*
 * handle ERL_SEND
 */
int handle_send(cnode_handler_t *phandler, erlang_msg * msg)
{
	int rt, backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;
	sip_msg_t tmsg;
	char route[]="erlang:self";
	ei_x_buff *request = &phandler->request;
	sr_xavp_t *xreq = NULL;
	str msg_name = str_init("msg");

	rt = route_get(&event_rt, route);
	if (rt < 0 || event_rt.rlist[rt] == NULL) {
		LM_WARN("ERL_SEND message not handled, missing event route %s\n", route);
		PRINT_DBG_REG_SEND(phandler->conn.nodename, msg->from, phandler->ec.thisnodename, msg->toname,request);
		return 0;
	}

	LM_DBG("executing self process %s\n", route);

	fmsg = faked_msg_next();
	memcpy(&tmsg, fmsg, sizeof(sip_msg_t));
	fmsg = &tmsg;

	if ((xreq = pv_xbuff_get_xbuff(&msg_name))) {
		LM_DBG("free previous value\n");
		xavp_destroy_list(&xreq->val.v.xavp);
	} else {
		xreq = xbuff_new(&msg_name);
	}

	if (!xreq) {
		LM_ERR("failed to create $xbuff(msg) variable\n");
		goto err;
	}

	/* decode request into $xbuff(msg) */
	xreq->val.type = SR_XTYPE_XAVP;

	/* XAVP <- ei_x_buff */
	if (erl_api.xbuff2xavp(&xreq->val.v.xavp,request)){
		LM_ERR("failed to decode message\n");
		goto err;
	}

	backup_rt = get_route_type();
	set_route_type(EVENT_ROUTE);
	init_run_actions_ctx(&ctx);
	run_top_route(event_rt.rlist[rt], fmsg, &ctx);
	set_route_type(backup_rt);

	free_xbuff_fmt_buff();
	xavp_destroy_list(xavp_get_crt_list());
	return 0;

err:
	free_xbuff_fmt_buff();
	xavp_destroy_list(xavp_get_crt_list());
	return -1;
}

int handle_req_ref_tuple(cnode_handler_t *phandler, erlang_msg * msg)
{
	erlang_ref ref;
	erlang_pid pid;
	int arity;
	ei_x_buff *request = &phandler->request;
	ei_x_buff *response = &phandler->response;

	ei_decode_tuple_header(request->buff, &request->index, &arity);

	if (ei_decode_ref(request->buff, &request->index, &ref))
	{
		LM_WARN("Invalid reference.\n");
		return -1;
	}

	if (ei_decode_pid(request->buff, &request->index, &pid))
	{
		LM_ERR("Invalid pid in a reference/pid tuple\n");
		return -1;
	}

	if (0)
	{
		ei_x_encode_atom(response, "ok");
	}
	else
	{
		ei_x_encode_tuple_header(response, 2);
		ei_x_encode_atom(response, "error");
		ei_x_encode_atom(response, "not_found");
	}

	return -1;
}

/* catch the response to ei_rpc_to (which comes back as {rex, {Ref, Pid}}
 The {Ref,Pid} bit can be handled by handle_ref_tuple
 */
int handle_rpc_response(cnode_handler_t *phandler, erlang_msg * msg, int arity)
{
	int type, size, arity2, tmpindex;
	ei_x_buff *request = &phandler->request;

	ei_get_type(request->buff, &request->index, &type, &size);
	switch (type)
	{
	case ERL_SMALL_TUPLE_EXT:
	case ERL_LARGE_TUPLE_EXT:
		tmpindex = request->index;
		ei_decode_tuple_header(request->buff, &tmpindex, &arity2);
		return handle_req_ref_tuple(phandler, msg);
	default:
		LM_ERR("Unknown RPC response.\n");
		break;
	}
	/* no reply */
	return -1;
}

int handle_msg_req_tuple(cnode_handler_t *phandler, erlang_msg * msg)
{
	char tupletag[MAXATOMLEN];
	int arity;
	int ret = 0;
	ei_x_buff *request = &phandler->request;

	ei_decode_tuple_header(request->buff, &request->index, &arity);
	if (ei_decode_atom(request->buff, &request->index, tupletag))
	{
		LM_ERR("error: badarg\n");
	}
	else
	{
		if (!strncmp(tupletag, "rex", MAXATOMLEN))
		{
			ret = handle_rpc_response(phandler, msg, arity);
		}
		else
		{
			LM_ERR("error: undef\n");
		}
	}
	return ret;
}

/* respond on net_adm:ping
 * e.g. message:
 *
 * {'$gen_call', {<tbe1@tbe.lan.343.0>, #Ref<194674.122.0>}, {is_auth, 'tbe1@tbe.lan'}} for net_kernel
 */
int handle_net_kernel(cnode_handler_t *phandler, erlang_msg * msg)
{
	int version, size, type, arity;
	char atom[MAXATOMLEN];
	erlang_ref ref;
	erlang_pid pid;
	ei_x_buff *request = &phandler->request;
	ei_x_buff *response = &phandler->response;

	/* start from first arg */
	request->index = 0;
	ei_decode_version(request->buff, &request->index, &version);
	ei_get_type(request->buff, &request->index, &type, &size);

	if (type != ERL_SMALL_TUPLE_EXT && type != ERL_SMALL_TUPLE_EXT)
	{
		LM_ERR("not a tuple\n");
		return -1;
	}

	ei_decode_tuple_header(request->buff, &request->index, &arity);

	if (arity != 3)
	{
		LM_ERR("wrong arity\n");
		return -1;
	}

	if (ei_decode_atom(request->buff, &request->index, atom) || strncmp(atom,
			"$gen_call", MAXATOMLEN))
	{
		LM_ERR("not atom '$gen_call'\n");
		return -1;
	}

	ei_get_type(request->buff, &request->index, &type, &size);

	if (type != ERL_SMALL_TUPLE_EXT && type != ERL_SMALL_TUPLE_EXT)
	{
		LM_ERR("not a tuple\n");
		return -1;
	}

	ei_decode_tuple_header(request->buff, &request->index, &arity);

	if (arity != 2)
	{
		LM_ERR("wrong arity\n");
		return -1;
	}

	if (ei_decode_pid(request->buff, &request->index, &pid)
			|| ei_decode_ref(request->buff, &request->index, &ref))
	{
		LM_ERR("decoding pid and ref error\n");
		return -1;
	}

	ei_get_type(request->buff, &request->index, &type, &size);

	if (type != ERL_SMALL_TUPLE_EXT)
	{
		LM_ERR("not a tuple\n");
		return -1;
	}

	ei_decode_tuple_header(request->buff, &request->index, &arity);

	if (arity != 2)
	{
		LM_ERR("bad arity\n");
		return -1;
	}

	if (ei_decode_atom(request->buff, &request->index, atom) || strncmp(atom,
			"is_auth", MAXATOMLEN))
	{
		LM_ERR("not is_auth\n");
		return -1;
	}

	/* To ! {Tag, Reply} */
	ei_x_encode_tuple_header(response, 2);
	ei_x_encode_ref(response, &ref);
	ei_x_encode_atom(response, "yes");

	ei_x_print_msg(response, &pid, 1);

	ei_send_tmo(phandler->sockfd, &pid, response->buff, response->index, CONNECT_TIMEOUT);

	return -1;
}

int erlang_whereis(cnode_handler_t *phandler,erlang_ref_ex_t *ref, erlang_pid *pid)
{
	ei_x_buff *response = &phandler->response;

	ei_x_encode_pid(response,&phandler->ec.self);

	return 0;
}

static int handle_erlang_calls(cnode_handler_t *phandler,erlang_ref_ex_t *ref, erlang_pid *pid, const char *method)
{
	ei_x_buff *response = &phandler->response;

	if (strcmp(method,"whereis")==0)
	{
		return erlang_whereis(phandler,ref,pid);
	}
	else {
		encode_error_msg(response, ref, "badrpc", "Method Not Found");
	}

	return 0;
}

/* handle rex calls
 *
 * example:
 *
 * {call, tbe, dlg_bye, [123, 456], <tbe1@tbe.lan.31.0>}
 *
 */
int handle_rex_call(cnode_handler_t *phandler,erlang_ref_ex_t *ref, erlang_pid *pid)
{
	char module[MAXATOMLEN];
	char method[MAXATOMLEN];
	char proc[2*MAXATOMLEN];
	erl_rpc_ctx_t ctx;
	rpc_export_t* exp;
	int arity;
	ei_x_buff *request = &phandler->request;
	ei_x_buff *response = &phandler->response;
	int size, type;

	/* already decoded {call,
	 * continue with
	 * module,method...}
	 */

	ei_get_type(request->buff,&request->index,&type,&size);
#ifdef ERL_SMALL_ATOM_EXT
	if (type == ERL_ATOM_EXT || type == ERL_SMALL_ATOM_EXT)
	{
#else
	if (type == ERL_ATOM_EXT)
	{
#endif
		if (ei_decode_atom(request->buff,&request->index,module))
		{
			encode_error_msg(response, ref, "error", "Failed to decode module name");
			return 0;
		}
	}
	else if (ei_decode_strorbin(request->buff, &request->index, MAXATOMLEN, module))
	{
		encode_error_msg(response, ref, "error", "Failed to decode module name");
		return 0;
	}

	ei_get_type(request->buff,&request->index,&type,&size);

#ifdef ERL_SMALL_ATOM_EXT
	if (type == ERL_ATOM_EXT || type == ERL_SMALL_ATOM_EXT)
	{
#else
	if (type == ERL_ATOM_EXT)
	{
#endif
		if (ei_decode_atom(request->buff,&request->index,method))
		{
			encode_error_msg(response, ref, "error", "Failed to decode method name");
			return 0;
		}
	}
	else if (ei_decode_strorbin(request->buff, &request->index, MAXATOMLEN, method))
	{
		encode_error_msg(response, ref, "error", "Failed to decode method name");
		return 0;
	}

	if (strcmp(module,"erlang") == 0)
	{
		/* start encoding */
		ei_x_encode_tuple_header(response, 2);
		if (ref->with_node)
		{
			ei_x_encode_tuple_header(response,2);
			ei_x_encode_ref(response, &ref->ref);
			ei_x_encode_atom(response,ref->nodename);
		}
		else {
			ei_x_encode_ref(response, &ref->ref);
		}

		return handle_erlang_calls(phandler,ref,pid,method);
	}

	/* be up to date with cfg */
	cfg_update();

	sprintf(proc,"%s.%s",module,method);

	exp=find_rpc_export(proc,0);

	if (!exp || !exp->function)
	{
		encode_error_msg(response, ref, "badrpc", "Method Not Found");

		return 0;
	}

	ei_get_type(request->buff,&request->index,&type,&size);

	/* open list for decoding */
	if (ei_decode_list_header(request->buff,&request->index,&arity))
	{
		LOG(L_ERR, "Expected list of parameters type=<%c> arity=<%d>\n", type, size);
		encode_error_msg(response, ref, "badarith", "Expected list of parameters.");
		return 0;
	}

	/* start encoding */
	ei_x_encode_tuple_header(response, 2);
	if (ref->with_node)
	{
		ei_x_encode_tuple_header(response,2);
		ei_x_encode_ref(response, &ref->ref);
		ei_x_encode_atom(response,ref->nodename);
	}
	else {
		ei_x_encode_ref(response, &ref->ref);
	}

	/* init context */
	ctx.phandler = phandler;
	ctx.pid = pid;
	ctx.ref = ref;
	ctx.response_sent = 0;
	ctx.request = request;
	ctx.request_index = request->index;
	ctx.response = response;
	ctx.reply_params = 0;
	ctx.tail = 0;
	ctx.fault = 0;
	ctx.fault_p = &ctx.fault;
	ctx.optional = 0;
	ctx.no_params = 0;
	ctx.response_index = response->index;
	ctx.size = arity;

	/* call rpc */
	exp->function(&erl_rpc_func_param,(void*)&ctx);

	if (ctx.no_params)
	{
		ei_x_encode_list_header(response,ctx.no_params);
	}

	if (erl_rpc_send(&ctx, 0))
	{
		response->index = ctx.response_index;
		ei_x_encode_atom(response, "error");
		ei_x_encode_tuple_header(response,2);
		ei_x_encode_string(response, "Inernal Error: Failed to encode reply");
	}
	else
	{
		ei_x_encode_empty_list(response);
	}

	empty_recycle_bin();

	/* we sent response so it's false for up calls */
	return 0;
}

/* {'$gen_call', {<tbe1@tbe.lan.14488.1>, #Ref<62147.65.0>}, {call, tbe, dlg_bye, [123, 456], <tbe1@tbe.lan.31.0>}} for rex */
int handle_rex_msg(cnode_handler_t *phandler, erlang_msg * msg)
{
	int version, size, type, arity;
	char atom[MAXATOMLEN];
	erlang_ref_ex_t ref;
	erlang_pid pid;
	ei_x_buff *request = &phandler->request;
	ei_x_buff *response = &phandler->response;

	/* start from first arg */
	request->index = 0;
	ei_decode_version(request->buff, &request->index, &version);
	ei_get_type(request->buff, &request->index, &type, &size);

	if (type != ERL_SMALL_TUPLE_EXT && type != ERL_SMALL_TUPLE_EXT)
	{
		LM_ERR("not a tuple\n");
		return -1;
	}

	ei_decode_tuple_header(request->buff, &request->index, &arity);

	if (arity != 3)
	{
		LM_ERR("wrong arity %d\n", arity);
		return -1;
	}

	if (ei_decode_atom(request->buff, &request->index, atom) || strncmp(atom,
			"$gen_call", MAXATOMLEN))
	{
		LM_ERR("not $gen_call\n");
		return -1;
	}

	ei_get_type(request->buff, &request->index, &type, &size);

	if (type != ERL_SMALL_TUPLE_EXT && type != ERL_SMALL_TUPLE_EXT)
	{
		LM_ERR("not a tuple\n");
		goto err;
	}

	ei_decode_tuple_header(request->buff, &request->index, &arity);

	if (arity != 2)
	{
		LM_ERR("wrong arity\n");
		goto err;
	}

	if (ei_decode_pid(request->buff, &request->index, &pid) )
	{
		LM_ERR("decoding pid error\n");
		goto err2;
	}

	ref.with_node = 0;
	/* we can got with host accompanied {#Ref<18224.110.0>, 'reg-two@pbx.lan'} */
	ei_get_type(request->buff, &request->index, &type, &size);
	if (type == ERL_REFERENCE_EXT || type == ERL_NEW_REFERENCE_EXT)
	{
		if (ei_decode_ref(request->buff, &request->index, &ref.ref))
		{
			LM_ERR("decoding ref error\n");
			goto err2;
		}
	}
	else if (type != ERL_SMALL_TUPLE_EXT)
	{
		LM_ERR("not a tuple type {#Ref<x.y.z>, 'node@host.tld'} %c\n", type);
		goto err;
	}
	else {
		if (ei_decode_tuple_header(request->buff, &request->index, &arity) || arity !=2)
			goto err2;

		if (ei_decode_ref(request->buff, &request->index, &ref.ref))
		{
			LM_ERR("decoding ref error\n");
			goto err2;
		}

		if (ei_decode_atom(request->buff, &request->index, ref.nodename))
		{
			LM_ERR("decoding node in ref error\n");
			goto err2;
		}
		ref.with_node = 1;
	}

	ei_get_type(request->buff, &request->index, &type, &size);

	if (type != ERL_SMALL_TUPLE_EXT)
	{
		LM_ERR("not a tuple\n");
		goto err;
	}

	ei_decode_tuple_header(request->buff, &request->index, &arity);

	if (arity != 5)
	{
		LM_ERR("bad arity %d\n", arity);
		goto err;
	}

	if (ei_decode_atom(request->buff, &request->index, atom) == 0 && strncmp(atom,
			"call", MAXATOMLEN) == 0)
	{
		return handle_rex_call(phandler, &ref, &pid);
	}

err:
	/* To ! {Tag, Reply} */
	ei_x_encode_tuple_header(response, 2);

	if (ref.with_node)
	{
		ei_x_encode_tuple_header(response,2);
		ei_x_encode_ref(response, &ref.ref);
		ei_x_encode_atom(response,ref.nodename);
	}
	else {
		ei_x_encode_ref(response, &ref.ref);
	}

	ei_x_encode_tuple_header(response,2);
	ei_x_encode_atom(response, "badrpc");
	ei_x_encode_string(response, "Unsupported rex request.");

	ei_x_print_msg(response, &pid, 1);

	ei_send_tmo(phandler->sockfd, &pid, response->buff, response->index, CONNECT_TIMEOUT);
err2:
	return -1;
}


int handle_erlang_msg(cnode_handler_t *phandler, erlang_msg * msg)
{
	int type, type2, size, version, arity, tmpindex;
	int ret = 0;
	ei_x_buff * request = &phandler->request;
	ei_x_buff * response = &phandler->response;
	erlang_pid from;

	if (msg->msgtype == ERL_REG_SEND )
	{
		cnode_reply_to_pid = &msg->from;

		if (!strncmp(msg->toname, "net_kernel",MAXATOMLEN)) {
			/* respond to ping stuff */
			ret = handle_net_kernel(phandler, msg);
		} else if (!strncmp(msg->toname, "rex",MAXATOMLEN)) {
			/* respond to rex stuff */
			ret = handle_rex_msg(phandler, msg);
		} else {
			/* try registered process */
			handle_reg_send(phandler,msg);
		}
	} else if (msg->msgtype == ERL_SEND) {
		ret = handle_send(phandler, msg);
	} else {
		/* TODO: fix below after adding #Pid and #Ref in PVs */
		request->index = 0;
		ei_decode_version(request->buff, &request->index, &version);
		ei_get_type(request->buff, &request->index, &type, &size);

		switch (type)
		{
		case ERL_SMALL_TUPLE_EXT:
		case ERL_LARGE_TUPLE_EXT:
			tmpindex = request->index;
			ei_decode_tuple_header(request->buff, &tmpindex, &arity);
			ei_get_type(request->buff, &tmpindex, &type2, &size);

			switch (type2)
			{
			case ERL_ATOM_EXT:
				ret = handle_msg_req_tuple(phandler, msg);
				break;
			case ERL_REFERENCE_EXT:
			case ERL_NEW_REFERENCE_EXT:
				ret = handle_req_ref_tuple(phandler, msg);
				break;
			case ERL_PID_EXT:
				ei_decode_pid(request->buff,&tmpindex,&from);
				ret = handle_send(phandler, msg);
				break;
			default:
				LM_ERR("nothing to do with term type=<%d> type2=<%d> -- discarding\n", type, type2);
				break;
			}
			break;
		default:
			LM_ERR("not handled term type=<%d> size=<%d> -- discarding\n", type, size);
			break;
		}
	}

	if (ret)
	{
		/* reset pid */
		cnode_reply_to_pid = NULL;
		return ret;
	}
	else if (response->index > 1 && cnode_reply_to_pid)
	{
		ei_x_print_msg(response, cnode_reply_to_pid, 1);

		if (ei_send(phandler->sockfd, cnode_reply_to_pid, response->buff, response->index))
		{
			LM_ERR("ei_send failed on node=<%s> socket=<%d>, %s\n",
					phandler->ec.thisnodename,phandler->sockfd, strerror(erl_errno));
		}

		/* reset pid */
		cnode_reply_to_pid = NULL;
		return ret;
	}
	else
	{
		LM_DBG("** no reply **\n");

		/* reset pid */
		cnode_reply_to_pid = NULL;
		return 0;
	}
}

void encode_error_msg(ei_x_buff *response, erlang_ref_ex_t *ref, const char *type, const char *msg )
{
	ei_x_encode_tuple_header(response, 2);

	if (ref->with_node)
	{
		ei_x_encode_tuple_header(response,2);
		ei_x_encode_ref(response, &ref->ref);
		ei_x_encode_atom(response,ref->nodename);
	}
	else {
		ei_x_encode_ref(response, &ref->ref);
	}

	ei_x_encode_tuple_header(response,2);
	ei_x_encode_atom(response, type);
	ei_x_encode_string(response, msg);
}
