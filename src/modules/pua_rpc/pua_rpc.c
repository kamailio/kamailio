/*
 * pua_rpc module - RPC pua module
 *
 * Copyright (C) 2014-2016 Juha Heinanen
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
 */

/*!
 * \file
 * \brief Kamailio pua_rpc :: rpc API interface for pua implementing pua.publish rpc command
 * Module: \ref pua_rpc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../core/sr_module.h"
#include "../../core/parser/parse_expires.h"
#include "../../core/dprint.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/str.h"
#include "../../core/mem/mem.h"
#include "../../core/pt.h"
#include "../../core/rpc_lookup.h"
#include "../../core/strutils.h"
#include "../pua/pua_bind.h"

MODULE_VERSION

static int mod_init(void);

static pua_api_t pua_rpc_api;

static const char *pua_rpc_publish_doc[2] = {
		"Send publish request and wait for the final reply, using a list "
		"of string parameters: presentity uri, expires, event package, "
		"content type, id, etag, outbound proxy, extra headers, "
		" and body (optional)",
		0};

static const char *pua_rpc_send_publish_doc[2] = {
		"Send publish request without waiting for the final reply, using a "
		"list "
		"of string parameters: presentity uri, expires, event package, "
		"content type, id, etag, outbound proxy, extra headers, "
		" and body (optional)",
		0};


static int pua_rpc_publish_callback(ua_pres_t *hentity, sip_msg_t *reply)
{
	rpc_delayed_ctx_t *dctx;
	void *c;
	void *st;
	rpc_t *rpc;
	struct hdr_field *hdr = NULL;
	int statuscode;
	int expires;
	int found;
	str etag;
	str reason = {0, 0};

	LM_DBG("running callback\n");

	if(reply == NULL || hentity == NULL || hentity->cb_param == NULL) {
		LM_ERR("NULL reply or hentity parameter\n");
		return -1;
	}

	dctx = (rpc_delayed_ctx_t *)(hentity->cb_param);
	hentity->cb_param = NULL;
	if(dctx == 0) {
		BUG("null delayed reply ctx\n");
		return -1;
	}
	rpc = &dctx->rpc;
	c = dctx->reply_ctx;

	if(reply == FAKED_REPLY) {
		statuscode = 408;
		reason.s = "Request Timeout";
		reason.len = strlen(reason.s);
	} else {
		statuscode = reply->first_line.u.reply.statuscode;
		reason = reply->first_line.u.reply.reason;
	}

	if(statuscode == 200) {
		if(rpc->add(c, "{", &st) < 0) {
			LM_ERR("rpc->add failed on '{'\n");
			rpc->delayed_ctx_close(dctx);
			return -1;
		}
		expires = ((exp_body_t *)reply->expires->parsed)->val;
		LM_DBG("expires = %d\n", expires);
		hdr = reply->headers;
		found = 0;
		while(hdr != NULL) {
			if(cmp_hdrname_strzn(&hdr->name, "SIP-ETag", 8) == 0) {
				found = 1;
				break;
			}
			hdr = hdr->next;
		}
		if(found == 0) {
			LM_ERR("SIP-ETag header field not found\n");
			rpc->delayed_ctx_close(dctx);
			return -1;
		}
		etag = hdr->body;
		LM_DBG("SIP-Etag = %.*s\n", etag.len, etag.s);
		rpc->struct_add(st, "S", "SIP-ETag", &etag);
		rpc->struct_add(st, "d", "Expires", expires);
	}

	rpc->delayed_ctx_close(dctx);

	return 0;
}


static void pua_rpc_publish_mode(rpc_t *rpc, void *c, int mode)
{
	str pres_uri, expires, event, content_type, id, etag, outbound_proxy,
			extra_headers, body;
	rpc_delayed_ctx_t *dctx = NULL;
	int exp, sign, ret, err_ret, sip_error;
	char err_buf[MAX_REASON_LEN];
	struct sip_uri uri;
	publ_info_t publ;

	body.s = 0;
	body.len = 0;
	dctx = 0;

	LM_DBG("rpc publishing ...\n");

	if((rpc->capabilities == 0)
			|| !(rpc->capabilities(c) & RPC_DELAYED_REPLY)) {
		rpc->fault(c, 600,
				"Reply wait/async mode not supported"
				" by this rpc transport");
		return;
	}

	ret = rpc->scan(c, "SSSSSSSS*S", &pres_uri, &expires, &event, &content_type,
			&id, &etag, &outbound_proxy, &extra_headers, &body);
	if(ret < 8) {
		rpc->fault(c, 400, "Too few or wrong type of parameters (%d)", ret);
		return;
	}

	if(parse_uri(pres_uri.s, pres_uri.len, &uri) < 0) {
		LM_ERR("bad resentity uri\n");
		rpc->fault(c, 400, "Invalid presentity uri '%s'", pres_uri.s);
		return;
	}
	LM_DBG("presentity uri '%.*s'\n", pres_uri.len, pres_uri.s);

	if(expires.s[0] == '-') {
		sign = -1;
		expires.s++;
		expires.len--;
	} else {
		sign = 1;
	}
	if(str2int(&expires, (unsigned int *)&exp) < 0) {
		LM_ERR("invalid expires parameter\n");
		rpc->fault(c, 400, "Invalid expires value '%s'", expires.s);
		return;
	}
	exp = exp * sign;
	LM_DBG("expires '%d'\n", exp);

	LM_DBG("event '%.*s'\n", event.len, event.s);

	LM_DBG("content type '%.*s'\n", content_type.len, content_type.s);

	LM_DBG("id '%.*s'\n", id.len, id.s);

	LM_DBG("ETag '%.*s'\n", etag.len, etag.s);

	LM_DBG("outbound_proxy '%.*s'\n", outbound_proxy.len, outbound_proxy.s);

	LM_DBG("extra headers '%.*s'\n", extra_headers.len, extra_headers.s);

	if(body.len > 0)
		LM_DBG("body '%.*s'\n", body.len, body.s);

	if((body.s == 0) && (content_type.len != 1 || content_type.s[0] != '.')) {
		LM_ERR("body is missing, but content type is not .\n");
		rpc->fault(c, 400, "Body is missing");
		return;
	}

	memset(&publ, 0, sizeof(publ_info_t));

	publ.pres_uri = &pres_uri;

	publ.expires = exp;

	publ.event = get_event_flag(&event);
	if(publ.event < 0) {
		LM_ERR("unknown event '%.*s'\n", event.len, event.s);
		rpc->fault(c, 400, "Unknown event");
		return;
	}

	if(content_type.len != 1) {
		publ.content_type = content_type;
	}

	if(!((id.len == 1) && (id.s[0] == '.'))) {
		publ.id = id;
	}

	if(!((etag.len == 1) && (etag.s[0] == '.'))) {
		publ.etag = &etag;
	}

	if(!((outbound_proxy.len == 1) && (outbound_proxy.s[0] == '.'))) {
		publ.outbound_proxy = &outbound_proxy;
	}

	if(!((extra_headers.len == 1) && (extra_headers.s[0] == '.'))) {
		publ.extra_headers = &extra_headers;
	}

	if(body.s != 0) {
		publ.body = &body;
	}

	if(mode == 1) {
		dctx = rpc->delayed_ctx_new(c);
		if(dctx == 0) {
			LM_ERR("internal error: failed to create context\n");
			rpc->fault(c, 500, "Internal error: failed to create context");
			return;
		}
		publ.cb_param = dctx;
		publ.source_flag = RPC_ASYN_PUBLISH;
	} else {
		publ.source_flag = RPC_PUBLISH;
	}

	ret = pua_rpc_api.send_publish(&publ);
	LM_DBG("pua send_publish returned %d\n", ret);

	if(mode == 1) {
		if(dctx->reply_ctx != 0) {
			/* callback was not executed or its execution failed */
			rpc = &dctx->rpc;
			c = dctx->reply_ctx;
		} else {
			return;
		}
	}

	if(ret < 0) {
		LM_ERR("pua send_publish failed\n");
		err_ret = err2reason_phrase(
				ret, &sip_error, err_buf, sizeof(err_buf), "RPC/PUBLISH");
		if(err_ret > 0) {
			rpc->fault(c, sip_error, "%s", err_buf);
		} else {
			rpc->fault(c, 500, "RPC/PUBLISH error");
		}
		if(mode == 1) {
			rpc->delayed_ctx_close(dctx);
		}
	}

	if(ret == 418) {
		rpc->fault(c, 500, "Wrong ETag");
		if(mode == 1) {
			rpc->delayed_ctx_close(dctx);
		}
	}

	return;
}

/**
 *
 */
static void pua_rpc_publish(rpc_t *rpc, void *c)
{
	pua_rpc_publish_mode(rpc, c, 1);
}

/**
 *
 */
static void pua_rpc_send_publish(rpc_t *rpc, void *c)
{
	pua_rpc_publish_mode(rpc, c, 0);
}

/**
 * rpc pua.subscribe
 *		<presentity_uri>
 *		<watcher_uri>
 *		<event_package>
 *		<expires>
 * */
static void pua_rpc_subscribe(rpc_t *rpc, void *ctx)
{
	int vexp = 0;
	str pres_uri;
	str watcher_uri;
	str event;
	struct sip_uri uri;
	subs_info_t subs;

	if(rpc->scan(ctx, "SSSd", &pres_uri, &watcher_uri, &event, &vexp) < 4) {
		rpc->fault(ctx, 500, "Not enough parameters");
		return;
	}
	if(parse_uri(pres_uri.s, pres_uri.len, &uri) < 0) {
		LM_ERR("bad presentity uri\n");
		rpc->fault(ctx, 400, "Invalid presentity URI");
		return;
	}
	if(parse_uri(watcher_uri.s, watcher_uri.len, &uri) < 0) {
		LM_ERR("bad watcher uri\n");
		rpc->fault(ctx, 400, "Invalid watcher URI");
		return;
	}
	LM_DBG("event '%.*s'\n", event.len, event.s);
	LM_DBG("expires '%d'\n", vexp);

	memset(&subs, 0, sizeof(subs_info_t));

	subs.pres_uri = &pres_uri;

	subs.watcher_uri = &watcher_uri;

	subs.contact = &watcher_uri;

	subs.expires = vexp;
	subs.source_flag |= RPC_SUBSCRIBE;
	subs.event = get_event_flag(&event);
	if(subs.event < 0) {
		LM_ERR("unknown event\n");
		rpc->fault(ctx, 404, "Unknown event");
		return;
	}

	if(pua_rpc_api.send_subscribe(&subs) < 0) {
		rpc->fault(ctx, 500, "Execution failure");
		return;
	}
}

static const char *pua_rpc_subscribe_doc[2] = {"Send subscribe request", 0};


rpc_export_t pua_rpc_ex[] = {
		{"pua.publish", pua_rpc_publish, pua_rpc_publish_doc, 0},
		{"pua.send_publish", pua_rpc_send_publish, pua_rpc_send_publish_doc, 0},
		{"pua.subscribe", pua_rpc_subscribe, pua_rpc_subscribe_doc, 0},
		{0, 0, 0, 0}};


/** module exports */
struct module_exports exports = {
		"pua_rpc",		 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		0,				 /* exported functions */
		0,				 /* exported parameters */
		pua_rpc_ex,		 /* RPC method exports */
		0,				 /* exported pseudo-variables */
		0,				 /* response handling function */
		mod_init,		 /* module initialization function */
		0,				 /* per-child init function */
		0				 /* module destroy function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	bind_pua_t bind_pua;

	LM_DBG("initializing\n");

	memset(&pua_rpc_api, 0, sizeof(pua_api_t));
	bind_pua = (bind_pua_t)find_export("bind_pua", 1, 0);

	if(!bind_pua) {
		LM_ERR("can't find pua\n");
		return -1;
	}

	if(bind_pua(&pua_rpc_api) < 0) {
		LM_ERR("can't bind pua\n");
		return -1;
	}

	if(pua_rpc_api.send_publish == NULL) {
		LM_ERR("could not import send_publish\n");
		return -1;
	}

	if(pua_rpc_api.register_puacb(
			   RPC_ASYN_PUBLISH, pua_rpc_publish_callback, NULL)
			< 0) {
		LM_ERR("could not register callback\n");
		return -1;
	}

	return 0;
}
