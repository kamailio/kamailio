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

#include <stdlib.h>
#include <stdio.h>
#include "mem.h"
#include "errcode.h"
#include "cb.h"

BRPC_STR_STATIC_INIT(er_intern, "Intern Error");
BRPC_STR_STATIC_INIT(er_sign, "No such call (signature mismatch)");
BRPC_STR_STATIC_INIT(er_nocall, "No such call");
BRPC_STR_STATIC_INIT(er_badmsg, "Malformed  message");

const static brpc_int_t ec_intern = BRPC_ESRV;
const static brpc_int_t ec_sign = BRPC_ESIGN;
const static brpc_int_t ec_nocall = BRPC_EMETH;
const static brpc_int_t ec_badmsg = BRPC_EMSG;

/* hash tables for replies & requests */
static ht_t *rpl_ht = NULL;
static ht_t *req_ht = NULL;

bool brpc_cb_init(size_t reqsz, size_t rplsz)
{
	if (! (reqsz + rplsz))
		WARN("callback initiate request with both reply and request "
				"subsystems disabled.\n");

	if (reqsz) {
		req_ht = ht_new(reqsz);
		if (! req_ht)
			goto error;
	}

	if (rplsz) {
		rpl_ht = ht_new(rplsz);
		if (! rpl_ht)
			goto error;
	}

	return true;

error:
	brpc_cb_close();
	return false;
}


void brpc_cb_close(void)
{
	if (req_ht)
		ht_del(req_ht);
	if (rpl_ht)
		ht_del(rpl_ht);
}


bool brpc_cb_rpl(brpc_t *req, brpc_cb_rpl_f callback, void *opaque)
{
	rpl_cb_t *rplcb;

	/* will use call's ID as hash value (probably incremental, so good for
	 * hash distribution. */
#ifndef NDEBUG /* should be optimized away by compiler, anyway, but still */
	if (sizeof(hval_t) < sizeof(brpc_id_t)) {
		ERR("BINRPC ID storage size exceeds hash value storage size.\n");
		abort();
	}
#endif /* NDEBUG */
	
	rplcb = brpc_calloc(1, sizeof(rpl_cb_t));
	if (! rplcb) {
		WERRNO(ENOMEM);
		return false;
	}
	HT_LINK_INIT(&rplcb->lnk, req->id);
	rplcb->cid = req->id;
	rplcb->cb_fn = callback;
	rplcb->opaque = opaque;
	if (! ht_ins_mx(rpl_ht, &rplcb->lnk)) {
		brpc_free(rplcb);
		return false;
	}
	return true;
}


__LOCAL void req_cb_del(req_cb_t *reqcb)
{
	if (! reqcb) {
		WARN("trying to dispose NULL request callback container.\n");
		return;
	}

	if (reqcb->name.val)
		brpc_free(reqcb->name.val);
	if (reqcb->sign.val)
		brpc_free(reqcb->sign.val);
	if (reqcb->doc.val)
		brpc_free(reqcb->doc.val);
	brpc_free(reqcb);
}

/* remove white spaces */
__LOCAL char *sign_trim(const char *argv, size_t *_newlen)
{
	char *fix, *pos;
	size_t len, newlen;
	bool run;

	assert(argv);

	len = strlen(argv) + /*0-term*/1;
	fix = (char *)brpc_malloc(len * sizeof(char));
	if (! fix) {
		WERRNO(ENOMEM);
		return NULL;
	}
	pos = fix;
	memcpy(pos, argv, len);

	newlen = 0;
	run = true;
	while (run) {
		switch (*pos) {
			case 0:
				newlen ++;
				run = false;
				continue;

			case ' ':
			case '\t':
				memcpy(pos, pos + 1, len - 1);
				break;

			default:
				pos ++;
				newlen ++;
		}
		len --;
	}

	*_newlen = newlen;
	DBG("argv '%s' trimmed to '%s' [%u].\n", argv, fix, newlen);
	return fix;
}

__LOCAL req_cb_t *req_cb_new(char *name, char *argv, char *doc)
{
	req_cb_t *reqcb;
	char *tmp = NULL;

	reqcb = (req_cb_t *)brpc_calloc(1, sizeof(req_cb_t));
	if (! reqcb) 
		goto enomem;

	reqcb->name.len = strlen(name) + /*0-term*/1;
	if (! reqcb->name.len) {
		WERRNO(EINVAL);
		ERR("invalid RPC name: empty.\n");
		goto error;
	}
	/* make sure the name remains available (caller can trash it after ret) */
	reqcb->name.val = (char *)brpc_malloc(reqcb->name.len * sizeof(char));
	if (! reqcb->name.val)
		goto enomem;
	memcpy(reqcb->name.val, name, reqcb->name.len);

	if (argv && (! (reqcb->sign.val = sign_trim(argv, &reqcb->sign.len))))
		goto enomem;

	if (doc) {
		reqcb->doc.len = strlen(doc);
		if (reqcb->doc.len) {
			reqcb->doc.val = (char *)brpc_malloc(reqcb->doc.len * 
					sizeof(char));
			if (! reqcb->doc.val)
				goto enomem;
			/* tdo: check for obscenities :-) */
			memcpy(reqcb->doc.val, doc, reqcb->doc.len);
		}
	}

	return reqcb;
enomem:
	WERRNO(ENOMEM);
error:
	if (reqcb)
		req_cb_del(reqcb);
	if (tmp)
		free(tmp);
	return NULL;
}

bool brpc_cb_req(char *name, char *sign, brpc_cb_req_f callback, char *doc,
		void *opaque)
{
	req_cb_t *reqcb, *match;
	ht_lnk_t *pos;
	struct brpc_list_head *tmp;
	bool failed;
	
	reqcb = req_cb_new(name, sign, doc);
	if (! reqcb)
		return false;

	/* make sure the sign's well formated */
	if (sign && (! brpc_repr_check(reqcb->sign.val, /*lax check*/false))) {
		ERR("invalid RPC signature `%.*s'\n", reqcb->sign.len, 
				reqcb->sign.val);
		goto error;
	}

	reqcb->cb_fn = callback;
	HT_LINK_INIT_STR(&reqcb->lnk, reqcb->name.val, reqcb->name.len);

	/* lock HT as one must make sure checkings are t-safe */
	if (HT_LOCK_GET(req_ht, HT_LINK_HVAL(&reqcb->lnk)) != 0) {
		WERRNO(ELOCK);
		goto error;
	}

	failed = false;
	/* check if name colisions exist */
	ht_for_hval(pos, req_ht, HT_LINK_HVAL(&reqcb->lnk), tmp) {
		match = ht_entry(pos, req_cb_t, lnk);
		if (match->name.len != reqcb->name.len)
			continue;
		if (strncmp(match->name.val, reqcb->name.val, reqcb->name.len) != 0)
			continue;

		/* mark colisions */
		DBG("Old RPC `%.*s' with new signature `%s'\n", reqcb->name.len, 
				reqcb->name.val, reqcb->sign);
		/* make sure both CBs can be distinguished by signature */
		/* TODO: we need strict representations to deal with
		 * colisions (for now, one can not guarantee signatures uniquness 
		 * otherwise). we only do it for colisions. */
		if (! brpc_repr_check(match->sign.val, /*strict*/ true)) {
			ERR("RPC name `%s' reused but (prior registation's) signature"
					" is lax: `%.*s'\n", name, 
					match->sign.len, match->sign.val);
			failed = true;
			goto unlock;
		}
		if (! brpc_repr_check(reqcb->sign.val, /*strict*/ true)) {
			ERR("RPC name `%s' reused but (current registation's) "
					"signature is lax: `%.*s'\n", name, 
					reqcb->sign.len, reqcb->sign.val);
			failed = true;
			goto unlock;
		}
		/* check uniqueness */
		if ((match->sign.len == reqcb->sign.len) &&
				(memcmp(match->sign.val, reqcb->sign.val, reqcb->sign.len)
					== 0)) {
			ERR("RPC callback <%s, %s> previously registered.\n", 
					name, sign);
			failed = true;
			goto unlock;
		}
		/* checks passed, mark need for deep check */
		match->deepchk = true;
		reqcb->deepchk = true;
	}
	
	reqcb->opaque = opaque;
	if (! ht_ins(req_ht, &reqcb->lnk))
		failed = true;

unlock:
	if (HT_LOCK_LET(req_ht, HT_LINK_HVAL(&reqcb->lnk)) != 0) {
		WERRNO(ELOCK);
		BUG("failed to relinquish lock for slot.\n");
		abort();
		/* formal, if no SIGABRT handling */
		goto error;
	}

	if (failed) {
error:
		req_cb_del(reqcb);
		return false;
	}

	DBG("added request callback for <%s, %s>.\n", name, sign);
	return true;
}

bool brpc_cb_req_rem(char *name, char *sign)
{
	ht_lnk_t *pos;
	struct brpc_list_head *tmp;
	hval_t hval;
	req_cb_t *match;
	size_t name_len, trim_len;
	char *trim, *trim_ptr;

	name_len = strlen(name) + /*0-term*/1;
	hval = hash_str(name, name_len);
	
	if (sign) {
		trim_ptr = sign_trim(sign, &trim_len);
		if (! trim_ptr) {
			ERR("failed to normalize signature description `%s' (%d:%s); "
					"trying to match call against original input.\n", sign, 
					brpc_errno, brpc_strerror());
			trim = sign;
			trim_len = strlen(trim) + /*0-term*/1;
		} else {
			trim = trim_ptr;
		}
	} else {
		trim_ptr = NULL;
#ifdef FIX_FALSE_GCC_WARNS
		trim = NULL;
		trim_len = 0;
#endif
	}

	ht_for_hval(pos, req_ht, hval, tmp) {
		match = ht_entry(pos, req_cb_t, lnk);
		if (match->name.len != name_len)
			continue;
		if (strncmp(match->name.val, name, name_len) != 0)
			continue;
		if (sign) {
			if (match->sign.len != trim_len)
				continue;
			if (strncmp(match->sign.val, trim, trim_len) != 0)
				continue;

			if (trim_ptr)
				brpc_free(trim_ptr); /* matching done, no longer needed */
		}

		
		if (! ht_rem_mx(req_ht, &match->lnk)) {
			WARN("request_callback <%s, %s> found, but removing"
					" failed (concurrently removing?)\n", name, sign);
			return false;
		}
		req_cb_del(match);
		DBG("request callback by <%s, %s> removed.\n", name, sign);
		return true;
	}
	if (trim_ptr)
		brpc_free(trim_ptr); /* no matching */
	DBG("failed to remove request callback by <%s, %s>: not found.\n", 
			name, sign);
	return false;
}


/* check if two strings are equal, case Insensitive (strncasecmp) */
__LOCAL bool strieq(const brpc_str_t *s1, const brpc_str_t *s2)
{
	unsigned long i;
	char *v1, *v2;
	if (s1->len != s2->len)
		return false;
	for (i = 0, v1 = s1->val, v2 = s2->val; i < s1->len; i ++) {
		if ((v1[i] | 0x20) != (v2[i] | 0x20))
			return false;
	}
	return true;
}

__LOCAL brpc_t *dispatch_req(brpc_t *req)
{
	hval_t hval;
	const brpc_str_t *mname;
	struct brpc_list_head *tmp;
	ht_lnk_t *pos;
	req_cb_t *reqcb;
	brpc_str_t repr;
	brpc_t *rpl;
	const brpc_str_t *ereason;
	const brpc_int_t *ecode;
	bool nmatched; /* RPC name matched */


	nmatched = false;
	brpc_errno = 0;
	mname = brpc_method(req);
	if (! mname) {
		if (brpc_errno == EBADMSG) {
			ereason = &er_badmsg;
			ecode = &ec_badmsg;
		} else {
			ereason = &er_intern;
			ecode = &ec_intern;
		}
		goto failed;
	}
	hval = hash_str(mname->val, mname->len);

	DBG("incomming request %.*s().\n", mname->len, mname->val);

	ht_for_hval(pos, req_ht, hval, tmp) {
		reqcb = ht_entry(pos, req_cb_t, lnk);
		if (mname->len != reqcb->name.len)
			continue;
		if (memcmp(mname->val, reqcb->name.val, mname->len) != 0)
			continue;
		/* we got a name match */
		nmatched = true;
		if (reqcb->deepchk) {
			repr.val = brpc_repr(req, &repr.len);
			if (! strieq(&reqcb->sign, &repr))
				continue;
		/* we got a signature match */
		}
		return reqcb->cb_fn(req, reqcb->opaque);
	}

	if (nmatched) {
		ereason = &er_sign;
		ecode = &ec_sign;
	} else {
		ereason = &er_nocall;
		ecode = &ec_nocall;
	}
failed:
	DBG("%.*s() fail ret: (%d, %.*s)", BRPC_STR_FMT(mname), *ecode, 
			ereason->len, ereason->val);
	rpl = brpc_rpl(req);
	if (! rpl) {
		ERR("failed to build fault reply.\n");
		return NULL;
	}
	if (! brpc_fault(rpl, ecode, ereason)) {
		/* TODO: it could actually be delivered w/o code/reason... */
		ERR("failed to set reply as fault.\n");
		brpc_finish(rpl);
		return NULL;
	}
	return rpl;
}


__LOCAL void rpl_cb_del(rpl_cb_t *rplcb)
{
	ht_rem_mx(rpl_ht, &rplcb->lnk);
	brpc_free(rplcb);
}

__LOCAL bool dispatch_by_cid(brpc_id_t cid, brpc_t *rpl)
{
	ht_lnk_t *pos;
	struct brpc_list_head *tmp;
	rpl_cb_t *match;
#ifndef NDEBUG
	bool called = false;;
#endif

	ht_for_hval(pos, rpl_ht, cid, tmp) {
		match = ht_entry(pos, rpl_cb_t, lnk);
#ifdef NDEBUG
		match->cb_fn(rpl, match->opaque);
		DBG("reply for request #%u received, callback invoked.\n", c>id);
		rpl_cb_del(match);
		return true;
#else /* NDEBUG */
		if (! called) {
			match->cb_fn(rpl, match->opaque);
			called = true;
			continue;
		}
		BUG("multiple HT hits for reply callback for request #%u!\n", 
				cid);
#endif /* NDEBUG */
	}
#ifndef NDEBUG
	if (called) {
		rpl_cb_del(match);
		return true;
	} else
#endif /* NDEBUG */
		DBG("reply for request #%u not matching any request (too late?).\n", 
				cid);
		return false;
}

bool brpc_cb_rpl_cancel(brpc_t *req)
{
	return dispatch_by_cid(req->id, NULL);
}

__LOCAL void dispatch_rpl(brpc_t *rpl)
{
	dispatch_by_cid(rpl->id, rpl);
}

brpc_t *brpc_cb_run(brpc_t *call)
{
	if (brpc_type(call) == BRPC_CALL_REQUEST)
		return dispatch_req(call);
	else {
		dispatch_rpl(call);
		return NULL;
	}
}

