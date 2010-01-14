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

#ifndef __BINRPC_DISSECTOR_H__
#define __BINRPC_DISSECTOR_H__

#include <stdbool.h>
#include "list.h"
#include "call.h"

enum BRPC_DISSECTOR_TYPE {
	BRPC_MSG_DISSECTOR,
	BRPC_VAL_DISSECTOR
};

typedef struct {
	struct brpc_list_head **stack; /**< every push inserts here */
	size_t cap; /**< how many slots in stack */
	ssize_t top; /**< where to make an insertion */
	struct brpc_list_head *cursor; /**< current position in current level */
	enum BRPC_DISSECTOR_TYPE type;
	union {
		brpc_t *msg; /**< original message */
		brpc_val_t *val; /* is it really needed? */
	};
	/* TODO: this practically doubles values tree => use one tree (?) */
	struct brpc_list_head list; /* refs to siblings */
	struct brpc_list_head head; /* refs to children */
} brpc_dissect_t;

brpc_dissect_t *brpc_msg_dissector(brpc_t *msg);
brpc_dissect_t *brpc_val_dissector(brpc_val_t *val);
void brpc_dissect_free(brpc_dissect_t *diss);
/**
 * Step into current RPC value; it can only succeed for set values (arrays,
 * maps, AVPs).
 */
bool brpc_dissect_in(brpc_dissect_t *diss);
/**
 * Step out into previous level.
 */
bool brpc_dissect_out(brpc_dissect_t *diss);
/**
 * Checks if there is a new value to inspect in current level.
 */
bool brpc_dissect_next(brpc_dissect_t *diss);
/**
 * Returns the number of RPC values in current level.
 */
size_t brpc_dissect_cnt(brpc_dissect_t *diss);
/**
 * Returns the current BINRPC value type.
 */
brpc_vtype_t brpc_dissect_seqtype(brpc_dissect_t *diss);
bool brpc_dissect_chain(brpc_dissect_t *anchor, brpc_dissect_t *cell);
size_t brpc_dissect_levcnt(brpc_dissect_t *diss);

#define brpc_dissect_fetch(_diss_)	\
		(const brpc_val_t *)_BRPC_VAL4LIST((_diss_)->cursor)

#define brpc_dissect_level(_diss_)	((const ssize_t)((_diss_)->top - 1))


#endif /* __BINRPC_DISSECTOR_H__ */
