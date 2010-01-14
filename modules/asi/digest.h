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

#ifndef __ASI_DIGEST_H__
#define __ASI_DIGEST_H__

#include <binrpc.h>

#include "parser/msg_parser.h"
#include "select.h"
#include "usr_avp.h"
#include "modules_s/xlog/xl_lib.h"

enum DIG_TOK_TYPE {
	DIG_TOKT_IMM,
	DIG_TOKT_SEL,
	DIG_TOKT_AVP,
	DIG_TOKT_XLL, /* TODO: still needed? */
};

typedef struct {
	enum DIG_TOK_TYPE type;
	/* Cloned received BINRPC value:
	 * 	- returned for DIG_TOKT_IMM type 
	 * 	- keeps avp_ident_t's strings
	 */
	brpc_val_t *spec;
	union {
		select_t *sel;
		avp_ident_t avp; //TODO: avp_spec_t == avp_ident_t !:?
		xl_elog_t *xll;
	};
} tok_dig_t;

struct digest_format {
	tok_dig_t *toks;
	size_t cnt;
};

typedef struct {
	str name;
	struct digest_format req;
	struct digest_format fin;
	struct digest_format prov;
} meth_dig_t;


enum ASI_DSGT_IDS {
	ASI_DGST_ID_REQ	= 1,
	ASI_DGST_ID_FIN	= 2,
	ASI_DGST_ID_PRV	= 3,
};



meth_dig_t *meth_array_new(brpc_str_t **names, size_t cnt);
void meth_array_free(meth_dig_t *array, size_t cnt);
int meth_add_digest(meth_dig_t *meth, brpc_str_t *ident, brpc_val_t *array);
int digest(struct sip_msg *sipmsg, brpc_t *rpcreq, 
		tok_dig_t *toks, size_t tokcnt);

#define STR_EQ_BSTR(_str_, _bstr_) \
	(((_str_)->len == (_bstr_)->len) && \
	(strncmp((_str_)->s, (_bstr_)->val, (_bstr_)->len) == 0))

#endif /* __ASI_DIGEST_H__ */
