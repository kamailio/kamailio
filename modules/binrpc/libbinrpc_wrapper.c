/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of sip-router, a free SIP server.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY IPTEGO GmbH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IPTEGO GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Bogdan Pintea.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../rpc.h"
#include "../../str.h"
#include "../../sr_module.h"

#include "libbinrpc_wrapper.h"

#define TX_TIMEOUT				10000
#define MAX_RPC_FAULT_REASON	256
#define MAX_RPC_PRINTF_BUF_LEN	256

#define CALL_ID_DESC	"(%s()#%d)"
#define CALL_ID_VAL(_req_)	brpc_method(_req_)->val, brpc_id(_req_)

typedef struct {
	brpc_t *req;
	brpc_dissect_t *diss; /* list of dissectors */
	struct reply_to sndto;
	brpc_t *rpl;
	bool sent; /* callback invoked rpc_send() */
	bool noreply; /* if true, callback did not add any value */
	enum BRPC_RUN_RET status; /* status of RPC call */
} rpc_ctx_t;


static int rpc_send(rpc_ctx_t* ctx);
static void rpc_fault(rpc_ctx_t* ctx, int code, char* fmt, ...);
static int rpc_add(rpc_ctx_t* ctx, char* fmt, ...);
static int rpc_scan(rpc_ctx_t *ctx, char* fmt, ...);
static int rpc_printf(rpc_ctx_t* ctx, char* fmt, ...);
static int rpc_struct_add(brpc_val_t* ctx, char* fmt, ...);
static int rpc_struct_scan(brpc_dissect_t *ctx, char* fmt, ...);
static int rpc_struct_printf(brpc_val_t* ctx, char* name, char* fmt, ...);

static rpc_t rpc_callbacks = {
	(rpc_fault_f)			rpc_fault,
	(rpc_send_f)			rpc_send,
	(rpc_add_f)				rpc_add,
	(rpc_scan_f)			rpc_scan,
	(rpc_printf_f)			rpc_printf,
	(rpc_struct_add_f)		rpc_struct_add,
	(rpc_struct_scan_f)		rpc_struct_scan,
	(rpc_struct_printf_f)	rpc_struct_printf
};

/* should a 200 reply be sent back if callback didn't do it? */
int force_reply = 1; 

static rpc_ctx_t *new_rpc_ctx(brpc_t *req, struct reply_to sndto)
{
	rpc_ctx_t *ctx;
	ctx = (rpc_ctx_t *)pkg_malloc(sizeof(rpc_ctx_t));
	if (! ctx) {
		ERR("out of pkg memory.\n");
		return NULL;
	}
	ctx->req = req;
	ctx->diss = NULL;
	ctx->sndto = sndto;
	
	ctx->rpl = brpc_rpl(req);
	if (! ctx->rpl) {
		ERR("failed to build reply framework for req %u: %s [%d].\n", 
				brpc_id(req), brpc_strerror(), brpc_errno);
		goto error;
	}

	ctx->sent = false;
	ctx->noreply = true;
	ctx->status = BRPC_RUN_UNKNOWN;
	return ctx;
error:
	pkg_free(ctx);
	return NULL;
}

void rpc_ctx_free(rpc_ctx_t *ctx)
{
	if (ctx->req)
		brpc_finish(ctx->req);
	if (ctx->rpl)
		brpc_finish(ctx->rpl);
	if (ctx->diss)
		brpc_dissect_free(ctx->diss);
	pkg_free(ctx);
}


enum BRPC_RUN_RET binrpc_run(brpc_t *req, struct reply_to sndto)
{
	const brpc_str_t *mname;
	rpc_export_t *rpc_export;
	rpc_ctx_t *ctx = NULL;
	enum BRPC_RUN_RET status;

	if (brpc_type(req) != BRPC_CALL_REQUEST) {
		ERR("received non request BINRPC call.\n");
		return BRPC_RUN_VOID;
	}

	mname = brpc_method(req);
	if (! mname) {
		ERR("failed to extract function call name: %s [%d].\n", 
				brpc_strerror(), brpc_errno);
		return BRPC_RUN_VOID;
	}

	ctx = new_rpc_ctx(req, sndto);
	if (! ctx) {
		ERR("failed go get a RPC requst context " CALL_ID_DESC ".\n",
				CALL_ID_VAL(req));
		return BRPC_RUN_VOID;
	}

	rpc_export = find_rpc_export(mname->val, /*flags*/0);
	if (! rpc_export) {
		ERR("no callback found for RPC request " CALL_ID_DESC ".\n", 
				CALL_ID_VAL(req));
		rpc_fault(ctx, BRPC_EMETH, "No such call (`%s').", mname->val);
		goto send; 
	} else {
		DEBUG("dispatching RPC request " CALL_ID_DESC ".\n", 
				CALL_ID_VAL(req));
		rpc_export->function(&rpc_callbacks, ctx);
	}
	
	if (! ctx->sent) {
		if (ctx->noreply) {
			if (force_reply)
				rpc_add(ctx, "ds", 200, "OK (auto).");
			else
				ctx->sent = true;
		}
send:
		rpc_send(ctx);
	}

	status = ctx->status;
	rpc_ctx_free(ctx);
	return status;
}

static int rpc_send(rpc_ctx_t* ctx)
{
	if (ctx->sent) {
		DEBUG("reply already sent.\n");
		return 0;
	}
	ctx->sent = true; /* prevent future attempts */

	if (! brpc_sendto(ctx->sndto.fd, /*destination*/&ctx->sndto.addr, 
			ctx->rpl, TX_TIMEOUT)) {
		ERR("error occured while sending reply " CALL_ID_DESC ".\n" ,
				CALL_ID_VAL(ctx->req));
		switch (brpc_errno) {
			case EINPROGRESS:
				ERR("...due to timing out while sending.\n");
				ctx->status = BRPC_RUN_ABORT;
				break;
			case EMSGSIZE:
				ERR("...due to reply message size.\n");
				/* TODO: send fault */
				ctx->status = BRPC_RUN_VOID;
				break;
			case ETIMEDOUT:
				ERR("...due to write timeout.\n");
				ctx->status = BRPC_RUN_VOID;
			default:
				ERR("...due to: %s [%d].\n", brpc_strerror(), brpc_errno);
				ctx->status = BRPC_RUN_VOID;
		}
	} else {
		DEBUG("call " CALL_ID_DESC " successfully replied.\n", 
				CALL_ID_VAL(ctx->req));
		ctx->status = BRPC_RUN_SUCCESS;
	}

	return (ctx->status == BRPC_RUN_SUCCESS) ? 0 : -1;
}

static void rpc_fault(rpc_ctx_t* ctx, int code, char* fmt, ...)
{
	va_list ap;
	char buff[MAX_RPC_FAULT_REASON];
	brpc_str_t reason = {buff, sizeof(buff)};

	if (ctx->sent) {
		BUG("trying to return fault, but a reply already dispatched "
				CALL_ID_DESC ".\n", CALL_ID_VAL(ctx->req));
		return;
	}
	if (brpc_is_fault(ctx->rpl)) {
		WARN("reply already set to fault.");
		return;
	}

	va_start(ap, fmt);
	reason.len = vsnprintf(buff, sizeof(buff), fmt, ap);
	va_end(ap);

	/* throw away all values that might have been added */
	if (! ctx->noreply) {
		brpc_finish(ctx->rpl);
		ctx->rpl = brpc_rpl(ctx->req);
		if (! ctx->rpl) {
			ERR("failed to (re)build failure framework " CALL_ID_DESC 
					": %s [%d].\n", CALL_ID_VAL(ctx->req),
					brpc_strerror(), brpc_errno);
			ctx->status = BRPC_RUN_VOID;
			return;
		}
	}
	ctx->noreply = false;

	if (! brpc_fault(ctx->rpl, (brpc_int_t *)&code, &reason)) {
		ERR("failed to signal request failure: %s [%d].\n", brpc_strerror(),
				brpc_errno);
		ctx->status = BRPC_RUN_VOID;
	}
}

static inline brpc_val_t *new_brpc_val(char desc, va_list *ap)
{
	brpc_int_t fp; /*floating point*/
	char *reason_c;
	str *reason_s;
	brpc_val_t *_val;

	switch (desc) {
		case 't':
		case 'b':
		case 'd':
			_val = brpc_int(va_arg(*ap, brpc_int_t));
			break;
		case 'f':
			/* TODO: support me */
			fp = (brpc_int_t)va_arg(*ap, double);
			WARN("floating point not supported, yet; faking as "
					"int=%d.\n", fp);
			_val = brpc_int(fp);
			break;
		case 's':
			reason_c = va_arg(*ap, char *);
			if (reason_c)
				_val = brpc_str(reason_c, strlen(reason_c)+/*0-term*/1);
			else
				_val = brpc_null(BRPC_VAL_STR);
			break;
		case 'S':
			reason_s = va_arg(*ap, str *);
			if (reason_s)
				_val = brpc_str(reason_s->s, reason_s->len);
			else
				_val = brpc_null(BRPC_VAL_STR);
			break;
		case '{':
			_val = brpc_map(NULL);
			*va_arg(*ap, brpc_val_t **) = _val;
			break;
		/*TODO: '[', '('*/
		default:
			ERR("invalid descriptor `%c'.\n", desc);
			return NULL;
	}
	return _val;
}

static int rpc_add(rpc_ctx_t* ctx, char* fmt, ...)
{
	brpc_val_t *val;
	va_list ap;
	char desc;

	if (brpc_is_fault(ctx->rpl)) {
		ERR("can not add values anymore: reply already set to fault.");
		return -1;
	}

	ctx->noreply = false;

	va_start(ap, fmt);
	while ((desc = *fmt)) {
		if (! (val = new_brpc_val(desc, &ap))) {
			ERR("failed to build BINRPC value; (descriptor: %s): %s [%d]", 
					fmt, brpc_strerror(), brpc_errno);
			goto error;
		}
		fmt ++;
		if (! brpc_add_val(ctx->rpl, val)) {
			ERR("failed to add BINRPC value to reply: %s [%d].\n", 
					brpc_strerror(), brpc_errno);
			brpc_val_free(val);
			return -1;
		}
	}
	va_end(ap);

	return 0;
error:
	rpc_fault(ctx, 500, "Failed to build response.");
	return -1;
}

static int rpc_struct_add(brpc_val_t *map, char* fmt, ...)
{
	brpc_val_t *avp_name = NULL, *avp_val, *avp /*4 GCC*/= NULL;
	va_list ap;
	char desc;
	char *member_name;

	va_start(ap, fmt);
	while ((desc = *fmt)) {
		member_name = va_arg(ap, char *);
		avp_name = brpc_cstr(member_name);
		if (! avp_name) {
			fprintf(stderr, "ERROR: failed to build structure member name"
					" value: %s [%d].\n", brpc_strerror(), brpc_errno);
			return -1;
		}

		if (! (avp_val = new_brpc_val(desc, &ap))) {
			ERR("failed to build BINRPC value; (descriptor: %s): %s [%d]", 
					fmt, brpc_strerror(), brpc_errno);
			goto error;
		}
		fmt ++;
	
		if (! (avp = brpc_avp(avp_name, avp_val))) {
			fprintf(stderr, "ERROR: failed to build BINRPC AVP: %s [%d].\n",
					brpc_strerror(), brpc_errno);
			goto error;
		}
	
		if (! brpc_map_add(map, avp)) {
			ERR("failed to add BINRPC AVP to MAP: %s [%d].\n", 
					brpc_strerror(), brpc_errno);
			goto error;
		}
	}
	va_end(ap);

	return 0;
error:
	if (avp) {
		brpc_val_free(avp);
	} else {
		if (avp_name)
			brpc_val_free(avp_name);
		if (avp_val)
			brpc_val_free(avp_val);
	}
	return -1;
}

static inline const brpc_val_t *extract_val(brpc_dissect_t *diss, 
		brpc_vtype_t expected_type)
{
	const brpc_val_t *val;
	if (! brpc_dissect_next(diss)) {
		ERR("scanned value not present in received message, at this point.\n");
		return NULL;
	}
	val = brpc_dissect_fetch(diss);
	if (brpc_val_type(val) != expected_type) {
		ERR("scanned value [%d] not of expected type in received "
				"message at this point [%d].\n", 
				expected_type, brpc_val_type(val));
		return NULL;
	}
	return val;
}

int brpc_scan(brpc_dissect_t *_diss, char *_fmt, va_list ap)
{
	;
	const brpc_val_t *val;
	str *string;
	int scanned;
	brpc_dissect_t *diss_ctx;
	
	scanned = 0;
	while (*_fmt) {
		switch (*_fmt ++) {
			/* TODO: support for structs scanning */
			case 't':
			case 'b':
			case 'd':
			case 'f': /* TODO: fixme */
				if(! (val = extract_val(_diss, BRPC_VAL_INT)))
					return -1;
				if (brpc_is_null(val)) {
					ERR("SER can't handle yet(?) null numeric values.\n");\
					return -1;
				}
				*va_arg(ap, int *) = brpc_int_val(val);
				break;
			case 'S':
				if(! (val = extract_val(_diss, BRPC_VAL_STR)))
					return -1;
				string = va_arg(ap, str *);
				if (brpc_is_null(val)) {
					ERR("SER can't handle properly (for now?) null str "
							"values; simulating by zero-len str.\n");
					string->s = NULL;
					string->len = 0;
				} else {
					string->s = brpc_str_val(val).val;
					string->len = brpc_str_val(val).len;
				}
				break;
			case 's':
				if(! (val = extract_val(_diss, BRPC_VAL_STR)))
					return -1;
				*va_arg(ap, char **) = brpc_str_val(val).val;
				break;
			case '{':
				if (! (val = extract_val(_diss, BRPC_VAL_MAP)))
					return -1;
				if (! (diss_ctx = brpc_val_dissector((brpc_val_t *)val))){\
					ERR("failed to get BINRPC value dissector: "
							"%s [%d].\n", brpc_strerror(), brpc_errno);
					return -1;
				}
				if (! brpc_dissect_chain(_diss, diss_ctx)) {
					ERR("failed to anchor BINRPC dissector: %s [%d].\n",
							brpc_strerror(), brpc_errno);
					brpc_dissect_free(diss_ctx);
					return -1;
				}
				*va_arg(ap, brpc_dissect_t **) = diss_ctx;
				break;
			/* TODO: '[', '(' */
			default:
				ERR("unsupported scan descriptor `%c'(0x%x)", 
						_fmt[-1], _fmt[-1]);
				return -1;
		}
		scanned ++;
	}
	va_end(ap);
	return scanned;
}

static int rpc_scan(rpc_ctx_t *ctx, char *fmt, ...)
{
	brpc_dissect_t *diss;
	va_list ap;
	int scanned;

	if ((! ctx->diss) && (! (ctx->diss = brpc_msg_dissector(ctx->req)))) {
		ERR("failed to get BINRPC message dissector: %s [%d].\n", 
				brpc_strerror(), brpc_errno);
		return -1;
	}
	diss = ctx->diss;

	va_start(ap, fmt);
	scanned = brpc_scan(diss, fmt, ap);
	va_end(ap);
	return scanned;
}

static int rpc_struct_scan(brpc_dissect_t *ctx, char* fmt, ...)
{
	int scanned;
	va_list ap;

	va_start(ap, fmt);
	scanned = brpc_scan(ctx, fmt, ap);
	va_end(ap);
	return scanned;
}


#define get_string(_fmt, _val) \
	do { \
		char buff[MAX_RPC_PRINTF_BUF_LEN]; \
		va_list ap; \
		int len; \
		\
		va_start(ap, fmt); \
		len = vsnprintf(buff, MAX_RPC_PRINTF_BUF_LEN, fmt, ap); \
		va_end(ap); \
		if (len < 0) { \
			ERR("failed to print: %s [%d].\n", strerror(errno), errno); \
			return -1; \
		} \
		if (! (_val = brpc_str(buff, len))) { \
			ERR("failed to build BINRPC string value: %s [%d].\n", \
					brpc_strerror(), brpc_errno); \
			return -1; \
		} \
	} while (0)

static int rpc_printf(rpc_ctx_t* ctx, char* fmt, ...)
{
	brpc_val_t *val;
	
	get_string(fmt, val);
	if (! brpc_add_val(ctx->rpl, val)) {
		ERR("failed to add BINRPC string value to reply: %s [%d].\n",
				brpc_strerror(), brpc_errno);
		brpc_val_free(val);
		return -1;
	}

	return 1;
}


static int rpc_struct_printf(brpc_val_t* ctx, char* name, char* fmt, ...)
{
	brpc_val_t *val;
	
	switch (ctx->type) {
		case BRPC_VAL_MAP:
			break;
		case BRPC_VAL_LIST:
		case BRPC_VAL_AVP:
			ERR("sequence value of type %d not yet (?) supported for this RPC "
					"API call.\n", ctx->type);
			return -1;
		default:
			BUG("illegal value as of type sequence: %d.\n", ctx->type);
			return -1;
	}

	get_string(fmt, val);
	if (! (brpc_map_add(ctx, val))) {
		ERR("failed to add BINRPC value to MAP: %s [%d].\n", brpc_strerror(),
				brpc_errno);
		brpc_val_free(val);
		return -1;
	}
	return 1;
}

