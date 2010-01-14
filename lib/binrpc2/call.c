/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of the BinRPC Library (libbinrpc).
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


#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>

#include "call.h"
#include "mem.h"
#include "errnr.h"
#include "list.h"
#include "log.h"
#include "misc.h"


#define PLOAD_LEN_LEN(byte2) \
	(((byte2 >> PKT_LL_OFF) & PKT_LL_MASK) + 1)
#define COOKIE_LEN(byte2) \
	((byte2 & PKT_CL_MASK) + 1)
#define list_add_val(_head_, _val_) \
		list_add_tail(&(_val_)->list, _head_)
#define _brpc_add_val(_call_, _val_) \
	do { \
		list_add_val(&(_call_)->vals.list, _val_); \
		(_call_)->vals.cnt ++; \
	} while (0)

#define _brpc_init(_call_) \
	do { \
		memset(_call_, 0, sizeof(brpc_t)); \
		INIT_LIST_HEAD(&(_call_)->vals.list); \
	} while (0)
#define val_list_next(_elem_)	(_elem_)->next


__LOCAL ssize_t header_len(const uint8_t *buff, size_t len)
{
	ssize_t ll, cl;

	if (len < HDR_START_BYTES)
		return -(ssize_t)(HDR_START_BYTES - len); /* read (-) this much more */

	ll = PLOAD_LEN_LEN(buff[HDR_LENS_OFF]);
	cl = COOKIE_LEN(buff[HDR_LENS_OFF]);

	return HDR_START_BYTES + ll + cl;
}

ssize_t brpc_pkt_len(const uint8_t *buff, size_t len)
{
	ssize_t hdr_len;
	size_t pload_len, ll;

	hdr_len = header_len(buff, len);
	if (hdr_len < 0)
		return hdr_len;
	if (len < hdr_len)
		return len - hdr_len; /* need (-) this much more, to tell */

	ll = PLOAD_LEN_LEN(buff[HDR_LENS_OFF]);
	pload_len = ntohz(buff + HDR_PLLEN_OFF, ll);
	return pload_len + hdr_len;
}

bool brpc_pkg_supported(uint8_t first_byte)
{
	unsigned int ver = first_byte & ((1 << VER_BITS) - 1);
	return (ver == BINRPC_PROTO_VER);
}

__LOCAL brpc_t *brpc_new(brpc_ctype_t type)
{
	brpc_t *ctx;
	ctx = (brpc_t *)brpc_calloc(1, sizeof(brpc_t));
	if (! ctx) {
		WERRNO(ENOMEM);
		return NULL;
	}
	ctx->type = type;
	INIT_LIST_HEAD(&ctx->vals.list);
	return ctx;
}

brpc_t *brpc_req(brpc_str_t _method, brpc_id_t id)
{
	brpc_val_t *method;
	brpc_t *req;

	if (! _method.len) {
		ERR("request's method name can not be NULL (id:%u).\n", id);
		WERRNO(EINVAL);
		return NULL;
	}
	req = brpc_new(BRPC_CALL_REQUEST);
	if (! req)
		return NULL;
	method = brpc_str(_method.val, _method.len);
	if (! method)
		return NULL;
	brpc_add_val(req, method);
	req->id = id;
	DBG("new BINRPC request %.*s(), #%u\n.", 
		BRPC_STR_FMT(&brpc_str_val(method)), id);
	return req;
}

bool brpc_fault(brpc_t *rpl, const brpc_int_t *_code, 
		const brpc_str_t *_reason)
{
	brpc_val_t *code, *reason;

	assert(rpl);
	if (rpl->type != BRPC_CALL_REPLY) {
		WERRNO(EINVAL);
		ERR("only replies can be marked as fault.\n");
		return false;
	}
	if (rpl->error) {
		WERRNO(EINVAL);
		ERR("reply alreay marked as fault indicator.\n");
		return false;
	}
	rpl->error = true;

	if (_code)
		code = brpc_int(*_code);
	else
		code = brpc_null(BRPC_VAL_INT);
	if (! code)
		goto error;
	brpc_add_val(rpl, code);
	
	if (_reason)
		reason = brpc_str(_reason->val, _reason->len);
	else
		reason = brpc_null(BRPC_VAL_STR);
	if (! reason)
		goto error;
	brpc_add_val(rpl, reason);

	return true;
error:
	brpc_finish(rpl);
	return false;
}

brpc_t *brpc_rpl(const brpc_t *req)
{
	brpc_t *rpl;
	rpl = brpc_new(BRPC_CALL_REPLY);
	if (! rpl)
		return NULL;
	rpl->id = req->id;
	return rpl;
}

void brpc_finish(brpc_t *call)
{
	struct brpc_list_head *k, *tmp;

	list_for_each_safe(k, tmp, &call->vals.list) {
		list_del(k);
		brpc_val_free(_BRPC_VAL4LIST(k));
	}

	if (call->nbuf.val)
		brpc_free(call->nbuf.val);

	brpc_free(call);
}

bool brpc_add_val(brpc_t *call, brpc_val_t *val)
{
	if (! list_empty(&val->list)) {
		WERRNO(EINVAL);
		ERR("can not add already added call value.\n");
		return false;
	}
	_brpc_add_val(call, val);
	return true;
}

brpc_val_t *brpc_fetch_val(brpc_t *call, size_t index)
{
	struct brpc_list_head *k;
	size_t idx = 0;

	if (index < 0) {
		WERRNO(EINVAL);
		return NULL;
	} else if (brpc_type(call) == BRPC_CALL_REQUEST)
		index ++; /* first is the method name */
	list_for_each(k, &call->vals.list) {
		if (idx == index) 
			return _BRPC_VAL4LIST(k);
		idx ++;
	}
	return NULL;
}

/**
 * Serializes the BINRPC
 * @param call The BINRPC to serialize
 * @return reference to container buffer; this buffer is handled by ??? (must
 * not be free'd!).
 */
const brpc_bin_t *brpc_serialize(brpc_t *call)
{
	uint8_t buff[BINRPC_MAX_PKT_LEN], *pos, *end;
	struct brpc_list_head *k;
	size_t blenlen, cklen, hdrlen, bodylen, tlen;
	unsigned int flags;

	/* some sanity checks: rpc origin and call consistency */
	if (call->locked) {
		WERRNO(EINVAL);
		ERR("trying to serialize locked call.\n");
		return NULL;
	}

	if (call->nbuf.val)
		return &call->nbuf;
	assert(call->nbuf.len == 0);

	pos = buff;
	end = buff + sizeof(buff);
	list_for_each(k, &call->vals.list) {
		pos = brpc_val_ser(_BRPC_VAL4LIST(k), pos, end);
		if (! pos)
			return NULL;
	}

	bodylen = pos - buff;
	if (! bodylen) {
		if (call->type == BRPC_CALL_REQUEST) {
			WERRNO(EINVAL);
			ERR("request #%u has no method name set.\n", call->id);
		}
	}
	blenlen = sizeofz(bodylen);
	cklen = sizeofz((size_t)call->id);
	/* is there space left for the header? */
	hdrlen = HDR_PLLEN_OFF + blenlen + cklen;
	if ((end - pos) < hdrlen) {
		WERRNO(ENOBUFS);
		return NULL;
	}
	DBG("pkg header len: %zd.\n", hdrlen);
	DBG("pkg body len: %zd.\n", bodylen);
	
	tlen = hdrlen + bodylen;
	call->nbuf.val = (uint8_t *)brpc_malloc(tlen * sizeof(uint8_t));
	if (! call->nbuf.val) {
		WERRNO(ENOMEM);
		return NULL;
	}
	call->nbuf.len = tlen;

	/* serialize header */
	/* first [| MAGIC  | VERS  |] byte */
	pos = (uint8_t *)call->nbuf.val;
	*pos = (BINRPC_PROTO_MAGIC << VER_BITS) | 
			(BINRPC_PROTO_VER & ((1 << VER_BITS) - 1));
	pos ++;
	/* second [| FLAGS  | LL | CL |] byte */
	if (call->type == BRPC_CALL_REQUEST)
		flags = BRPC_FLG_REQUEST;
	else if (call->error)
		flags = BRPC_FLG_ERROR;
	else
		flags = 0;
	*pos = flags << PKT_FLAGS_OFF;

	assert(blenlen < (1 << (PKT_FLAGS_OFF - PKT_LL_OFF)));
	*pos |= (blenlen - 1) << PKT_LL_OFF;
	assert((cklen - 1) < (1 << PKT_LL_OFF));
	*pos |= cklen - 1;
	pos ++;
	/* lenght and cookie fields */
	pos += htonz(pos, bodylen);
	pos += htonz(pos, (size_t)call->id);

	/* copy serialized body */
	memcpy(pos, buff, bodylen);
	
	return &call->nbuf;
}

brpc_t *brpc_raw(uint8_t *buff, size_t len)
{
	ssize_t hdrlen;
	size_t bodylen, ll, cl;
	uint8_t *pos;
	unsigned int flags;
	brpc_t *call;

	assert(buff);

	hdrlen = header_len(buff, len);
	if ((hdrlen < 0) || (len < hdrlen)) {
		WERRNO(EMSGSIZE);
		return false;
	}
	DBG("new raw packet (len :%zd).\n", len);

	pos = buff;
	assert((*pos & ((1 << VER_BITS) - 1)) == BINRPC_PROTO_VER);
	pos ++;

	flags = *pos >> PKT_FLAGS_OFF;
	call = brpc_new((flags & BRPC_FLG_REQUEST) ? BRPC_CALL_REQUEST : 
			BRPC_CALL_REPLY);
	if (! call)
		return NULL;
	call->locked = true;
	call->nbuf.val = buff;
	call->nbuf.len = len;
	
	if (flags & BRPC_FLG_ERROR) {
		if (call->type == BRPC_FLG_REQUEST) {
			WERRNO(EBADMSG);
			ERR("error flag can only be set for replies.\n");
			goto error;
		}
		call->error = true;
	}

	if (flags & (BRPC_FLG_2 | BRPC_FLG_3))
		WARN("message using reserved flags #2&#3.\n");
		
	ll = PLOAD_LEN_LEN(*pos);
	cl = COOKIE_LEN(*pos);
	pos ++;

	bodylen = ntohz(pos, ll);
	pos += ll;

	DBG("pkg header len: %zd.\n", hdrlen);
	DBG("pkg body len: %zd.\n", bodylen);

	if (len < hdrlen + bodylen) {
		WERRNO(EMSGSIZE);
		goto error;
	}

	call->id = (uint32_t)ntohz(pos, cl);
	pos += cl;
	call->pos = pos;

	return call;
error:
	brpc_finish(call);
	return NULL;
}


brpc_t *brpc_deserialize(uint8_t *buff, size_t len)
{
	uint8_t *own;

	if (! (own = brpc_malloc(len * sizeof(uint8_t)))) {
		brpc_errno = ENOMEM;
		return NULL;
	}
	memcpy(own, buff, len);
	return brpc_raw(own, len);
}

/* some sanity checks */
__LOCAL bool check_fault(brpc_t *call, unsigned cnt)
{
	brpc_val_t *new_val, *tmp1, *tmp2;
	brpc_vtype_t type;

	switch (cnt) {
		default:
			/* let's try to ignore the rest, but expect first 2 to be corr. */
			WARN("multiple records (%u) in error message: considering "
					"only first two.\n", cnt);
			/* no break; */
		case 2:
			tmp1 = _BRPC_VAL4LIST(call->vals.list.next);
			tmp2 = _BRPC_VAL4LIST(call->vals.list.next->next);
			switch (tmp1->type) {
				case BRPC_VAL_INT:
					/* I expect the next to be string */
					switch (tmp2->type) {
						case BRPC_VAL_STR: 
							return true;;
						default:
							WERRNO(EBADMSG);
							ERR("unexpected type (%u) as fault reason "
									"value.\n", tmp2->type);
							return false;
					}
					break;
				case BRPC_VAL_STR:
					switch (tmp2->type) {
						case BRPC_VAL_INT:
							/* values present, but wrong order: put int 1st */
							list_del(&tmp2->list); /* del int */
							list_add(&tmp2->list, &call->vals.list); /* 1st */
							INFO("reordered fault values (int, str).\n");
							return true;
						default:
							WERRNO(EBADMSG);
							ERR("unexpected type (%u) as fault code "
									"value.\n", tmp2->type);
							return false;
							
					}
					break;
				default:
					type = tmp1->type;
			}
			break;
		
		case 1:
			tmp1 = _BRPC_VAL4LIST(call->vals.list.next);
			switch (tmp1->type) {
				case BRPC_VAL_INT:
					new_val = brpc_null(BRPC_VAL_STR);
					if (! new_val)
						return false;
					_brpc_add_val(call, new_val);
					INFO("added null value as fault reason.\n");
					return true;
				case BRPC_VAL_STR:
					new_val = brpc_null(BRPC_VAL_INT);
					if (! new_val)
						return false;
					/* insert 1st */
					list_add(&new_val->list, &call->vals.list); 
					INFO("added null value as fault code.\n");
					return true;
				default:
					type = tmp1->type;
			}
			break;

		case 0:
			WARN("no fault explanation.\n");
			tmp1 = brpc_null(BRPC_VAL_INT);
			tmp2 = brpc_null(BRPC_VAL_STR);
			if ((! tmp1) || (! tmp2))
				return false;
			_brpc_add_val(call, tmp1);
			_brpc_add_val(call, tmp2);
			return true;
	}

	WERRNO(EBADMSG);
	ERR("unexpected type (%u) as fault value.\n", type);
	return false;
}

bool brpc_unpack(brpc_t *call)
{
	uint8_t *end;
	brpc_val_t *new_val;

	if (! call->locked) {
		WERRNO(EINVAL);
		ERR("can not unpack not locked call #%u.\n", call->id);
	}

	end = call->nbuf.val + call->nbuf.len;
	if (end <= call->pos) {
		DBG("call #%u already unpacked.\n", call->id);
		return true;
	}

	if (call->type == BRPC_CALL_REQUEST)
		if (! brpc_unpack_method(call))
			return false;

	while (call->pos < end) {
		new_val = brpc_val_deser(&call->pos, end);
		if (! new_val)
			goto error;
		_brpc_add_val(call, new_val);
	}

	if (call->error && (! check_fault(call, call->vals.cnt))) {
		ERR("invalid fault reply.\n");
		goto error;
	}

	return true;
error:
	return false;
}

bool brpc_unpack_method(brpc_t *req)
{
	brpc_val_t *meth;

	if (req->type != BRPC_CALL_REQUEST) {
		WERRNO(EINVAL);
		ERR("can not parse method name for reply (#%u).\n", req->id);
		return false;
	}

	if (! list_empty(&req->vals.list)) {
		DBG("request's #%u method name already unpacked.\n", req->id);
		return true;
	}

	meth = brpc_val_deser(&req->pos, req->nbuf.val + req->nbuf.len);
	if (! meth) {
		WERRNO(EBADMSG);
		ERR("request #%u lacks method name.\n", req->id);
		return false;
	}
	_brpc_add_val(req, meth);
	if (meth->type != BRPC_VAL_STR) {
		WERRNO(EBADMSG);
		ERR("req flaged as request but first value's type (%u) isn't "
				"string.\n", meth->type);
		return false;
	}

	return true;
}

const brpc_str_t *brpc_method(brpc_t *req)
{
	if (req->type != BRPC_CALL_REQUEST) {
		WERRNO(EINVAL);
		ERR("req not a request (%d).\n", req->type);
		return NULL;
	}
	if (req->locked)
		if (! brpc_unpack_method(req))
			return false;
	return &brpc_str_val(_BRPC_VAL4LIST(req->vals.list.next));
}

bool brpc_fault_status(brpc_t *rpl, brpc_int_t **_code, 
		brpc_str_t **_reason)
{
	brpc_val_t *c, *r;
	if (! rpl->error) {
		WERRNO(EINVAL);
		ERR("rpl not a fault.\n");
		return false;
	}

	if (rpl->locked)
		if (! brpc_unpack(rpl))
			return false;

	c = _BRPC_VAL4LIST(rpl->vals.list.next);
	r = _BRPC_VAL4LIST(rpl->vals.list.next->next);

	if (c->null)
		*_code = NULL;
	else
		*_code = &brpc_int_val(c);

	if (r->null)
		*_reason = NULL;
	else
		*_reason = &brpc_str_val(r);

	return true;
}

bool brpc_asm(brpc_t *call, const char *fmt, ...)
{
	va_list ap;
	ssize_t sp, i;
	brpc_val_t **recstack, *rec;
	brpc_val_t lst;
	char spec;
	brpc_vtype_t closed;
	bool ret;
	brpc_vtype_t type;
	char *c_arg;
	brpc_int_t *i_arg;
	brpc_str_t *s_arg;
	brpc_bin_t *b_arg;
	void **v_args; /* arguments packed into array */
	ssize_t v_pos;
	bool use_va; /* use va_arg; if !, use v_args */


	if (call->locked) {
		WERRNO(EINVAL);
		ERR("can not pack into locked BINRPC.\n");
		return false;
	}

	/* normally, strlen/2 should do, but guard agaisnt "<<<<<" bugs */
	recstack = (brpc_val_t **)brpc_malloc(strlen(fmt) * sizeof(brpc_val_t *));
	if (! recstack) {
		WERRNO(ENOMEM);
		return false;
	}
	sp = 0;

	/* fake call into a list of values */
	memset(&lst, 0, sizeof(brpc_val_t));
	INIT_LIST_HEAD(&lst.val.seq.list);
	lst.type = BRPC_VAL_LIST;

	ret = false;

#define PUSH(lst) \
	do { \
		assert(sp < strlen(fmt)); \
		recstack[sp++] = lst; \
	} while (0)
#define POP(expect) \
	do { \
		if (! sp --) { \
			WERRNO(EINVAL); \
			ERR("invalid format descriptor; understack before `%s'.\n",fmt); \
			goto error; \
		} \
		if (recstack[sp]->type != expect) { \
			WERRNO(EFMT); \
			ERR("invalid format specified (expected to close %d instead of" \
					" %d); jammed at `%c%s'.\n", recstack[sp]->type, \
					expect, spec, fmt); \
			goto error; \
		} \
	} while(0)
#ifndef NDEBUG
#	define TOP	(sp ? recstack[sp-1] : NULL)
#else
#	define TOP	recstack[sp - 1]
#endif
#define OVER	recstack[sp] /* only used after POP */

#define ADD2SEQ(rec) \
	do { \
		bool (*add)(brpc_val_t *, brpc_val_t *); \
		switch (TOP->type) { \
			case BRPC_VAL_AVP: add = brpc_avp_add; break;\
			case BRPC_VAL_MAP: add = brpc_map_add; break;\
			case BRPC_VAL_LIST: add = brpc_list_add; break;\
			default: \
				BUG("illegal type (%u) as sequence.\n", TOP->type); \
				goto error; \
		} \
		if (! add(TOP, rec)) \
			goto error; \
	} while (0)
/* argument fetch */
#define AFETCH(type) \
	((use_va) ? va_arg(ap, type) : (type)v_args[v_pos ++])
/* integer fetch */
#define IAFETCH(type) \
	((use_va) ? va_arg(ap, type) : (type)(intptr_t)v_args[v_pos ++])

#ifdef FIX_FALSE_GCC_WARNS
	v_pos = 0;
	v_args = NULL;
#endif
	spec = 0;
	PUSH(&lst);
	use_va = true;
	va_start(ap, fmt);
	while (*fmt) {
		switch ((spec = *fmt++)) {
			case '!':
				if (! use_va) {
					WERRNO(EINVAL);
					ERR("`!' used twice in format descriptor; jammed at "
							"`%c%s'.\n", spec, fmt);
				}
				use_va = false;
				v_args = va_arg(ap, void **);
				v_pos = 0;
				DBG("switching to array arguments.\n");
				break;

			do {
			case '<': rec = brpc_empty_avp(); break;
			case '[': rec = brpc_list(NULL); break;
			case '{': rec = brpc_map(NULL); break;
			} while (0); 
				if (! rec)
					goto error;
				PUSH(rec);
				break;
			
			do {
			case '>': closed = BRPC_VAL_AVP; break;
			case ']': closed = BRPC_VAL_LIST; break;
			case '}': closed = BRPC_VAL_MAP; break;
			} while (0);
				POP(closed);
				if (list_empty(&OVER->val.seq.list)) {
					WARN("empty %d set at`%c%s'\n", closed, spec, fmt);
					brpc_val_free(TOP);
				} else {
					ADD2SEQ(OVER);
				}
				break;

			do {
			case 'c':
				c_arg = AFETCH(char *);
				rec = (c_arg) ? brpc_cstr(c_arg) : brpc_null(BRPC_VAL_STR);
				break;
			case 'd':
				rec = brpc_int(IAFETCH(int));
				break;
			case 'i':
				i_arg = AFETCH(brpc_int_t *);
				rec = (i_arg) ? brpc_int(*i_arg) : brpc_null(BRPC_VAL_INT);
				break;
			case 's': 
				s_arg = AFETCH(brpc_str_t *);
				rec = (s_arg) ? brpc_str(s_arg->val, s_arg->len) : 
						brpc_null(BRPC_VAL_STR);
				break;
			case 'b':
				b_arg = AFETCH(brpc_bin_t *);
				rec = (b_arg) ? brpc_bin(b_arg->val, b_arg->len) : 
						brpc_null(BRPC_VAL_BIN);
				break;
			} while (0);
				if (! rec)
					goto error;
				ADD2SEQ(rec);
				break;

			do {
			case 'I': type = BRPC_VAL_INT; break;
			case 'S': type = BRPC_VAL_STR; break;
			case 'B': type = BRPC_VAL_BIN; break;
			case 'A': type = BRPC_VAL_AVP; break;
			case 'M': type = BRPC_VAL_MAP; break;
			case 'L': type = BRPC_VAL_LIST; break;
			} while (0);
				rec = AFETCH(brpc_val_t *);
				if (rec) {
					if (rec->type != type) {
						WERRNO(EINVAL);
						ERR("expected value of type '%d'; got type '%d'.\n",
								type, rec->type);
						goto error;
					}
					if (rec->locked) {
						rec = brpc_val_clone(rec);
						if (! rec)
							goto error;
					}
				} else {
					rec = brpc_null(type);
					if (! rec)
						goto error;
				}
				ADD2SEQ(rec);
				break;

			case '\t':
			case ':':
			case ',':
			case ' ': break; /* better visibility */
			default:
				WERRNO(EINVAL);
				ERR("unsupported format specifier `%c'; jammed at "
						"`%c%s'.\n", spec, spec, fmt);
				goto error;
		}
	}
	va_end(ap);
	POP(BRPC_VAL_LIST);

	if (sp) {
		WERRNO(EINVAL);
		ERR("invalid format specified: unclosed groups.\n");
		goto error;
	}

	list_splice_tail(&lst.val.seq.list, &call->vals.list);
	
	ret = true;
error:
	for (i = /*@0: lst*/1; i < sp; i ++)
		brpc_val_free(recstack[i]);
	brpc_free(recstack);
	return ret;

#undef PUSH
#undef POP
#undef TOP
#undef ADD2SEQ
#undef AFETCH
}


bool brpc_dsm(brpc_t *call, const char *fmt, ...)
{
	va_list ap;
	char spec;
	struct brpc_list_head **headstack, **posstack, *crrhead, *crrpos;
	size_t sksz;
	ssize_t sp;
	bool ret, skip;
	brpc_val_t *rec;
	brpc_vtype_t type;
	void **v_args;
	ssize_t v_pos;
	register enum {USE_VAARG, VOID_LEVEL2, VOID_LEVEL1} use_vargs;

	if (brpc_is_fault(call)) {
		ERR("will not disassamble faulted call.\n");
		WERRNO(EINVAL);
		return false;
	}

	if (call->locked)
		if (! brpc_unpack(call))
			return false;

	/*
	 * How deep the stack can be.
	 * The parser ensures a correct packet, so it's safe to assume the worst
	 * case as `[[... [x] ...]]'.
	 */
	sksz = strlen(fmt) / 2 + /* round up */1;
	DBG("unpack stack size: %zd.\n", sksz);

	headstack = (struct brpc_list_head **)brpc_malloc(sksz * 
			sizeof(struct brpc_list_head *));
	posstack = (struct brpc_list_head **)brpc_malloc(sksz * 
			sizeof(struct brpc_list_head *));
	if ((! headstack) || (! posstack)) {
		WERRNO(ENOMEM);
		return false;
	}
	sp = 0;

	crrhead = &call->vals.list;
	crrpos = crrhead;

#define LPUSH(newhead) \
	do { \
		assert(sp < sksz); \
		headstack[sp] = crrhead; \
		posstack[sp] = crrpos; \
		sp ++; \
		crrhead = newhead; \
		crrpos = newhead; \
	} while (0)
#define LPOP \
	do { \
		if (! sp) { \
			WERRNO(EINVAL); \
			ERR("illegal format specified (group closing without beginning);"\
					" jammed at `%c%s'.\n", spec, fmt); \
			goto error; \
		} \
		sp --; \
		crrhead = headstack[sp]; \
		crrpos = posstack[sp]; \
	} while (0)
#define LNEXT	crrpos = crrpos->next
#define LFETCH(expect, rec) \
	do { \
		LNEXT; \
		if (crrpos == crrhead) { /* did the list cycle? */ \
			WERRNO(EFMT); \
			ERR("list value ended; unpacking failed at `%c%s'.\n", \
					spec, fmt); \
			goto error; \
		} \
		rec = _BRPC_VAL4LIST(crrpos); \
		if (rec->type != expect) { \
			WERRNO(EFMT); \
			ERR("format-type mismatch; expected: %d; encountered: %d; " \
					"jammed at `%c%s'.\n", expect, rec->type, spec, fmt); \
			goto error; \
		} \
	} while (0)
#define AASIGN(basic_type, val) \
	do { \
		switch (use_vargs) { \
			case USE_VAARG: *va_arg(ap, basic_type **) = val; break; \
			case VOID_LEVEL1: v_args[v_pos++] = (void *)val; break; \
			case VOID_LEVEL2: *(basic_type **)v_args[v_pos++] = val; break; \
		} \
	} while (0)

#ifdef FIX_FALSE_GCC_WARNS
	v_pos = 0;
#endif

	if (call->type == BRPC_CALL_REQUEST)
		/* go past method name */
		LNEXT;
	ret = false;
	use_vargs = USE_VAARG;
	v_args = NULL;
	va_start(ap, fmt);
	while ((spec = *fmt++)) {
		DBG("decoding a `%c'.\n", spec);
		switch (spec) {
			case '!': 
				/* LEV1: void[x] will contain reference to value */
				switch (use_vargs) {
					case VOID_LEVEL2:
						DBG("switching void levels: 2 -> 1\n");
					case USE_VAARG:
						use_vargs = VOID_LEVEL1;
						v_args = va_arg(ap, void **);
						v_pos = 0;
						break;
					case VOID_LEVEL1:
						WARN("`!' reused in expression: ignoring instance "
								"at `%c%s'.\n", spec, fmt);
						break;
				}
				DBG("switching to array arguments; void level 1.\n");
				break;

			case '&': 
				/* LEV2: the value in void[x] will contain ref to value */
				switch (use_vargs) {
					case VOID_LEVEL1:
						DBG("switching void levels: 1 -> 2\n");
					case USE_VAARG:
						use_vargs = VOID_LEVEL2;
						v_args = va_arg(ap, void **);
						v_pos = 0;
						break;
					case VOID_LEVEL2:
						WARN("`&' reused in expression: ignoring instance "
								"at `%c%s'.\n", spec, fmt);
						break;
				}
				DBG("switching to array arguments; void level 2.\n");
				break;


			do {
			case '<': type = BRPC_VAL_AVP; break;
			case '[': type = BRPC_VAL_LIST; break;
			case '{': type = BRPC_VAL_MAP; break;
			} while (0);
				LFETCH(type, rec);
				LPUSH(&rec->val.seq.list);
				break;

			do {
			case '>': type = BRPC_VAL_AVP; break;
			case ']': type = BRPC_VAL_LIST; break;
			case '}': type = BRPC_VAL_MAP; break;
			} while (0);
				rec = _BRPC_VAL4SEQ(crrhead);
				if (rec->type != type) {
					WERRNO(EINVAL);
					ERR("invalid format: expected: %d; received: %d; jammed"
							" at `%c%s'.\n", rec->type, type, spec, fmt);
					goto error;
				}
				LPOP;
				break;
			
			case '*':
				skip = true;
				do {
					switch ((spec = *fmt++)) {
						case ' ': break;
						case ')':
						case ']':
						case '}':
						case 0:
							fmt --; /* re-read it in main loop */
							skip = false;
							break;
						default:
							WERRNO(EINVAL);
							ERR("invalid format: unsupported specifier `%c'"
									" after `*'", spec);
							goto error;
					}
				} while (skip);
				break;

			case 'c': 
				LFETCH(BRPC_VAL_STR, rec);
				AASIGN(char, (rec->null) ? NULL : rec->val.str.val);
				break;
			case 'd':
				LFETCH(BRPC_VAL_INT, rec);
				AASIGN(int, (rec->null) ? NULL : (int *)&rec->val.int32);
				break;
			case 'i':
				LFETCH(BRPC_VAL_INT, rec);
				AASIGN(brpc_int_t, (rec->null) ? NULL : &rec->val.int32);
				break;
			case 's':
				LFETCH(BRPC_VAL_STR, rec);
				AASIGN(brpc_str_t, (rec->null) ? NULL : &rec->val.str);
				break;
			case 'b':
				LFETCH(BRPC_VAL_BIN, rec);
				AASIGN(brpc_bin_t, (rec->null) ? NULL : &rec->val.bin);
				break;

			do {
			case 'I': type = BRPC_VAL_INT; break;
			case 'S': type = BRPC_VAL_STR; break;
			case 'B': type = BRPC_VAL_BIN; break;
			case 'A': type = BRPC_VAL_AVP; break;
			case 'L': type = BRPC_VAL_LIST; break;
			case 'M': type = BRPC_VAL_MAP; break;
			} while (0);
				LFETCH(type, rec);
				AASIGN(brpc_val_t, rec);
				break;

			case '.': LNEXT; break;
			case '\t':
			case ':':
			case ',':
			case ' ': break;
			default:
				WERRNO(EINVAL);
				ERR("illegal descriptor '%c'(0x%x); jammed at `%c%s'.\n", 
						spec, spec, spec, fmt);
				goto error;
		}
	}
	va_end(ap);

	if (sp) {
		WERRNO(EINVAL);
		ERR("invalid specifier: group(s) not closed; last opened: %u.\n", 
				_BRPC_VAL4SEQ(crrhead)->type);
		goto error;
	}
	ret = true;
error:
	brpc_free(headstack);
	brpc_free(posstack);
	return ret;

#undef LPUSH
#undef LPOP
#undef LFETCH
#undef LNEXT
#undef AASIGN
}

char *brpc_repr(brpc_t *call, size_t *len)
{
	brpc_str_t repr;
	ssize_t pos;
	struct brpc_list_head *head;

	if (call->error) {
		WERRNO(EINVAL);
		ERR("can not build representation for errornous reply.\n");
		return NULL;
	}

	if (call->locked)
		if (! brpc_unpack(call))
			return false;

	pos = 0;
	memset(&repr, 0, sizeof(brpc_str_t));
	head = &call->vals.list;
	if (! brpc_vals_repr(head, &repr, &pos))
		return NULL;

	if (repr.val) {
		repr.val[pos] = 0; /* always place for 0-term */
		DBG("repr[%u]: <%s>\n", pos, repr.val);
		/* first char will always be 's', for requests (due to method name) */
		if (call->type == BRPC_CALL_REQUEST)
			memcpy(repr.val, repr.val + 1, repr.len - 1);
	} else {
		DBG("empty representation");
	}

	if (len)
		*len = pos;

	return repr.val;
}


bool brpc_repr_check(const char *repr, bool strict)
{
	/* TODO: implement a generic stack (possible?) */
	brpc_vtype_t *stack;
	unsigned int sp;
	bool passed;
	bool run;
	size_t repr_len;

	DBG("checking signature `%s'.\n", repr);
	if (! repr)
		return false;
	repr_len = strlen(repr);
	if (! repr_len)
		/* empty repr. are well formated */
		return true;

	/* normally, strlen/2 should do, but guard agaisnt "<<<<<" bugs */
	stack = (brpc_vtype_t *)brpc_malloc(repr_len * sizeof(brpc_vtype_t));
	if (! stack) {
		WERRNO(ENOMEM);
		return false; /* actually, a false indicator (the repr might be OK) */
	}
	stack[0] = 0; /* prevent ")" from accidentally working */
	sp = 0;

#define PUSH(sym) \
	do { \
		assert(sp < repr_len); \
		stack[sp++] = sym; \
	} while (0)
#define POP(sym) \
	do { \
		if ((! sp) || (stack[--sp] != sym)) { \
			passed = false; \
			goto clean; \
		} \
	} while (0)

	passed = true;
	run = true;
	while (run)
		switch (*repr ++) {
			case 'c':
			case 'd':
			case 'i':
			case 's':
			case 'b':
			case 'I':
			case 'S':
			case 'B':
				/* allowed */
				break;

			case 'A':
			case 'M':
			case 'L':
			case '.':
			case '*':
			case ' ':
				if (strict) {
					DBG("illegal char `%c' for strict RPC representation.\n", 
							*(repr - 1));
					passed = false;
					goto clean;
				}
				/* allowed if not strict */
				break;

			case '[': PUSH(BRPC_VAL_LIST); break;
			case ']': POP(BRPC_VAL_LIST); break;
			case '<': PUSH(BRPC_VAL_AVP); break;
			case '>': POP(BRPC_VAL_AVP); break;
			case '{': PUSH(BRPC_VAL_MAP); break;
			case '}': POP(BRPC_VAL_MAP); break;

			case 0:
				run = false;
				break;

			default:
				DBG("illegal char `%c' in RPC representation.\n", 
						*(repr - 1));
				passed = false;
				goto clean;
		}

	if (sp)
		passed = false;

clean:
	brpc_free(stack);
	return passed;
#undef PUSH
#undef POP
}


ssize_t brpc_val_cnt(brpc_t *msg)
{
	ssize_t res;

	if (msg->locked)
		if (! brpc_unpack(msg))
			return -1;
	
	res = msg->vals.cnt;
	if (brpc_type(msg) == BRPC_CALL_REQUEST)
		res --;
	return res;
}
