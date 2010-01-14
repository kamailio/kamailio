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

#include <stdlib.h>

#include "strutil.h"
#include "binds.h"
#include "digest.h"

enum DIG_TOKEN_TYPE_MARK {
	DIG_TOKM_SEL	= '@',
	DIG_TOKM_AVP	= '$',
	DIG_TOKM_XLL	= '%',
};

/*TODO: mod param*/
#define XLL_MAX_BUFF	1024
#ifdef XLL_NULL_FIX
#define XLL_NULL		"<null>"
#endif
#define TM_SEL_HTID		"@tm.htid"

meth_dig_t *meth_array_new(brpc_str_t **names, size_t cnt)
{
	int k;
	meth_dig_t *array = NULL;

	if (! (array = (meth_dig_t *)pkg_malloc(cnt * sizeof(meth_dig_t)))) {
		ERR("out of pkg memory (meth dig array).\n");
		goto end;
	}
	/* must: checked later @see meth_free() */
	memset((char *)array, 0, cnt * sizeof(meth_dig_t));

	for (k = 0; k < cnt; k ++) {
		DEBUG("supported SIP method: %.*s.\n", (int)names[k]->len, 
				names[k]->val);
		if (strclo(names[k]->val, names[k]->len, &array[k].name, 
				STR_CLONE_PKG|STR_CLONE_TRIM) < 0) {
			ERR("failed to clone BINRPC to SER string: %s [%d].\n",
					strerror(errno), errno);
			goto end;
		}
	}
	return array;
end:
	if (array)
		pkg_free(array);
	return NULL;
}



static inline void tok_free(tok_dig_t *tok)
{
	if (tok->spec)
		brpc_val_free(tok->spec);

	switch (tok->type) {
		case DIG_TOKT_SEL: free_select(tok->sel); break;
		case DIG_TOKT_AVP: free_avp_ident(&tok->avp); break;
		case DIG_TOKT_XLL: xl_elogs_free(tok->xll); break;
		default:
			; /*ignore*/
	}
}


static void meth_free(meth_dig_t *meth)
{
	int j;

	pkg_free(meth->name.s);
	
	for (j = 0; j < meth->req.cnt; j ++)
		tok_free(&meth->req.toks[j]);
	if (j)
		pkg_free(meth->req.toks);

	for (j = 0; j < meth->fin.cnt; j ++)
		tok_free(&meth->fin.toks[j]);
	if (j)
		pkg_free(meth->fin.toks);

	for (j = 0; j < meth->prov.cnt; j ++)
		tok_free(&meth->prov.toks[j]);
	if (j)
		pkg_free(meth->prov.toks);
}


void meth_array_free(meth_dig_t *array, size_t cnt)
{
	int i;
	for (i = 0; i < cnt; i ++)
		meth_free(&array[i]);
	if (i)
		pkg_free(array);
}


static int tok_fix(tok_dig_t *tok, const brpc_val_t *_spec)
{
	bool resolve = true;
	brpc_str_t spec_brpc;
	str spec_ser;
	switch (brpc_val_type(_spec)) {
		case BRPC_VAL_LIST:
		case BRPC_VAL_AVP:
		case BRPC_VAL_MAP:
		case BRPC_VAL_BIN:
		case BRPC_VAL_INT:
		case BRPC_VAL_FLOAT:
			DEBUG("non-string BINRPC type [%d] as digest token specifier: will"
					" be passed back as is.\n", brpc_val_type(_spec));
			resolve = false;
			/* for rest (=strings), the type can only be determined when 
			 * resolving */
			tok->type = DIG_TOKT_IMM;
			/* no break */
		case BRPC_VAL_STR:
			if (! (tok->spec = brpc_val_clone(_spec))) {
				ERR("failed to clone specification BINRPC value: %s [%d].\n",
						brpc_strerror(), brpc_errno);
				return -1;
			}
			if (brpc_is_null(tok->spec)) {
				tok->type = DIG_TOKT_IMM;
				resolve = false;
				WARN("received NULL digest string token: will be returned as"
						" is to requestor.\n");
			}
			break;
		default:
			BUG("unexpected BINRPC value type %d.\n", brpc_val_type(_spec));
			return -1;
	}

	if (! resolve) /* pretty much done */
		return 0;

	spec_brpc = brpc_str_val(tok->spec);
	DEBUG("resolving digest token `%.*s'.\n", BRPC_STR_FMT(&spec_brpc));

	/* TODO: check escaping: @@, $$, %% */
	switch (spec_brpc.val[0]) {
		case DIG_TOKM_SEL: 
			tok->type = DIG_TOKT_SEL;
			/* TODO: shoudln't the shm_* version be better used? (size) */
			return parse_select(&spec_brpc.val, &tok->sel);
		case DIG_TOKM_AVP:
			tok->type = DIG_TOKT_AVP;
			spec_ser.s = spec_brpc.val + /* skip leading `$' */1;
			spec_ser.len = spec_brpc.len - /*$*/1 - /*0-term*/1;
			return parse_avp_ident(&spec_ser, &tok->avp);
		case DIG_TOKM_XLL: 
			tok->type = DIG_TOKT_XLL;
			return xl_parse(spec_brpc.val, &tok->xll);
		default:
			/* the value in tok->spec will be simply returned to caller */
			tok->type = DIG_TOKT_IMM;
	}

	return 0;
}


int meth_add_digest(meth_dig_t *meth, brpc_str_t *ident, brpc_val_t *array)
{
	brpc_dissect_t *diss = NULL;
	int id;
	tok_dig_t ** digest;
	size_t *dlen /*4 GCC*/= NULL;
	char *tmp;
	size_t cnt;
	const brpc_val_t *spec;
	/* Sip Method Type, Tm Ht Id specifiers */
	brpc_val_t *smt_spec = NULL, *thi_spec = NULL;
	int idx = 0;
	int res = -1;
	
	if (! ident) {
		ERR("NULL identifier provided in digest specification.\n");
		goto end;
	}
	/* identify what current digest is for */
	if ((id = (int)strtol(ident->val, NULL, /*decimal*/0)) == 0) {
		ERR("failed to decode \"%s\" as digest type identifier (%s).\n",
				ident->val, strerror(errno));
		goto end;
	}
	switch (id) {
		do {
			case ASI_DGST_ID_REQ: 
				digest = &meth->req.toks; 
				dlen = &meth->req.cnt;
				tmp = "requests";
				break;
			case ASI_DGST_ID_FIN:
				digest = &meth->fin.toks; 
				dlen = &meth->fin.cnt;
				tmp = "final replies";
				break;
			case ASI_DGST_ID_PRV:
				digest = &meth->prov.toks; 
				dlen = &meth->prov.cnt;
				tmp = "provisional replies";
				break;
		} while (0);
			if ((*dlen) || (*digest)) {
				ERR("digest format for %s [%d] specified twice.\n", 
						tmp, id);
				goto end;
			}
			break;
		default:
			ERR("invalid digest identifier `%d'.\n", id);
			goto end;
	}

	if ((cnt = brpc_val_seqcnt(array)) == 0) {
		INFO("empty digest format received for %s for SIP method "
				"\"%.*s\".\n", tmp, STR_FMT(&meth->name));
		/* don't allow it specified for two times */
		*digest = (tok_dig_t *)-1;
		goto ok;
	} else {
		if (! (*digest = (tok_dig_t *)pkg_malloc((/*id*/1 + cnt) * 
				sizeof(tok_dig_t)))) {
			ERR("out of pkg memory (digest).\n");
			goto end;
		}
	}

	/* unpack array values */
	if (! (diss = brpc_val_dissector(array))) {
		ERR("failed to get dissector for digest array: %s [%d].\n",
				brpc_strerror(), brpc_errno);
		goto end;
	}
	for (; brpc_dissect_next(diss); idx ++) {
		/* assert(idx < *dlen); */
		spec = brpc_dissect_fetch(diss);
		if (tok_fix((*digest) + idx, (brpc_val_t *)spec) < 0) {
			goto end;
		}
	}
	DEBUG("specified %d digest tokens for %s for %.*s.\n", idx, tmp, 
			STR_FMT(&meth->name));
ok:
	res = 0;
end:
	*dlen = idx;
	if (diss)
		brpc_dissect_free(diss);
	/* the values are cloned by tok_fix() */
	if (smt_spec)
		brpc_val_free(smt_spec);
	if (thi_spec)
		brpc_val_free(thi_spec);
	return res;
}

int digest(struct sip_msg *sipmsg, brpc_t *rpcreq, 
		tok_dig_t *toks, size_t tokcnt)
{
	int i;
	brpc_val_t *rpcval;
	union {
		struct {
			char buff[XLL_MAX_BUFF];
			int len;
		} xll;
		str sel;
		struct {
			avp_t *cont;
			avp_value_t val;
		} avp;
	} res;

	DEBUG("adding %zd digest tokens as RPC arguments.\n", tokcnt);
	for (i = 0; i < tokcnt; i ++) {
		switch (toks[i].type) {
			case DIG_TOKT_IMM:
				rpcval = brpc_val_clone(toks[i].spec);
				break;
			case DIG_TOKT_SEL:
				if (run_select(&res.sel, toks[i].sel, sipmsg) < 0) {
					DEBUG("digesting token `%.*s' failed.\n", 
							BRPC_STR_FMT(&brpc_str_val(toks[i].spec)));
					res.sel.s = NULL;
				}
				if (res.sel.s)
					rpcval = brpc_str(res.sel.s, res.sel.len);
				else
					rpcval = brpc_null(BRPC_VAL_STR);
				break;
			case DIG_TOKT_AVP:
				if ((res.avp.cont = search_first_avp(toks[i].avp.flags, 
						toks[i].avp.name, &res.avp.val, 
						/* no search state */NULL))) {
					if (res.avp.cont->flags & AVP_VAL_STR)
						rpcval = brpc_str(res.avp.val.s.s, res.avp.val.s.len);
					else
						rpcval = brpc_int(res.avp.val.n);
				} else {
					rpcval = brpc_null(BRPC_VAL_STR);
				}
				break;
			case DIG_TOKT_XLL:
				res.xll.len = XLL_MAX_BUFF;
				if (xl_print(sipmsg, toks[i].xll, res.xll.buff, 
						&res.xll.len) < 0) {
					ERR("digesting token `%.*s' failed.\n", 
							BRPC_STR_FMT(&brpc_str_val(toks[i].spec)));
					goto end;
				}
				if (res.xll.len) {
#ifdef XLL_NULL_FIX
					if (strncmp(res.xll.buff, XLL_NULL, sizeof(XLL_NULL) - 
							/*0-term*/1) == 0)
						rpcval = brpc_null(BRPC_VAL_STR);
					else
#endif
						rpcval = brpc_str(res.xll.buff, res.xll.len);
				} else {
					rpcval = brpc_null(BRPC_VAL_STR);
				}
				break;
			default:
				BUG("unexpected token type %d (idx: %d/%zd).\n", toks[i].type,
						i, tokcnt);
				abort();
		}
		if (! rpcval) {
			ERR("failed to build BINRPC digest value: %s [%d].\n",
					brpc_strerror(), brpc_errno);
			goto end;
		}
		if (! brpc_add_val(rpcreq, rpcval)) {
			ERR("failed to add BINRPC value to request: %s [%d].\n",
					brpc_strerror(), brpc_errno);
#ifdef EXTRA_DEBUG
			/* most probably a segfault will occur later anyway, but still */
			abort();
#endif
			brpc_val_free(rpcval);
			goto end;;
		}
		DEBUG("digest tok #%i: added new val (%d) for token (%d).\n", i,
				brpc_val_type(rpcval), toks[i].type);
	}
end:
	return i - (int)tokcnt;
}
