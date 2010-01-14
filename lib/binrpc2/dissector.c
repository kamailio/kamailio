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


#include "mem.h"
#include "errnr.h"
#include "config.h"
#include "misc.h"
#include "log.h"
#include "dissector.h"


__LOCAL brpc_dissect_t* dissector_alloc(struct brpc_list_head *chain, 
		enum BRPC_DISSECTOR_TYPE type)
{
	brpc_dissect_t *diss;

	diss = (brpc_dissect_t *)brpc_calloc(1, sizeof(brpc_dissect_t));
	if (! diss) {
		WERRNO(ENOMEM);
		return NULL;
	}
	diss->cap = BINRPC_DISSECTOR_SSZ;
	diss->stack = (struct brpc_list_head **)brpc_malloc(diss->cap * 
			sizeof(struct brpc_list_head *));
	if (! diss->stack) {
		WERRNO(ENOMEM);
		goto error;
	}

	diss->stack[0] = chain;
	diss->top = 1;
	diss->cursor = chain;
	diss->type = type;
	INIT_LIST_HEAD(&diss->list);
	INIT_LIST_HEAD(&diss->head);

	return diss;
error:
	brpc_free(diss);
	return NULL;
}

brpc_dissect_t *brpc_msg_dissector(brpc_t *msg)
{
	brpc_dissect_t *diss;

	if (msg->locked)
		if (! brpc_unpack(msg))
			return false;

	diss = dissector_alloc(&msg->vals.list, BRPC_MSG_DISSECTOR);
	if (! diss)
		return NULL;
	diss->msg = msg;
	if (msg->type == BRPC_CALL_REQUEST)
		/* go past method name */
		diss->cursor = diss->cursor->next;

	return diss;
}

brpc_dissect_t *brpc_val_dissector(brpc_val_t *val)
{
	brpc_dissect_t *diss;

	switch (val->type) {
		case BRPC_VAL_LIST:
		case BRPC_VAL_AVP:
		case BRPC_VAL_MAP:
			break;
		default:
			ERR("can not have a dissector for BINRPC value of non-sequence"
					" type %d.\n", val->type);
			WERRNO(EINVAL);
			return NULL;
	}

	diss = dissector_alloc(&val->val.seq.list, BRPC_VAL_DISSECTOR);
	if (! diss)
		return NULL;
	diss->val = val;
	return diss;
}


void brpc_dissect_free(brpc_dissect_t *diss)
{
	struct brpc_list_head *k, *tmp;
	list_for_each_safe(k, tmp, &diss->head) {
		list_del(k);
		brpc_dissect_free(brpc_list_entry(k, brpc_dissect_t, list));
	}
	brpc_free(diss->stack);
	brpc_free(diss);
}

bool brpc_dissect_in(brpc_dissect_t *diss)
{
	brpc_val_t *val;
	struct brpc_list_head **realloced, *head;

	val = _BRPC_VAL4LIST(diss->cursor);
	/* kind of double check, as the caller already did this type checking... */
	switch (val->type) {
		case BRPC_VAL_AVP:
		case BRPC_VAL_MAP:
		case BRPC_VAL_LIST:
			break;
		default:
			ERR("can not dissect_in BINRPC value of non-sequence type %d.\n", 
					val->type);
			WERRNO(EINVAL);
			return false;
	}

	if (diss->top == diss->cap) { /* is there space to 'push'? */
		realloced = brpc_realloc(diss->stack, 2 * diss->cap);
		if (! realloced) {
			WERRNO(ENOMEM);
			return false;
		}
		diss->stack = realloced;
		diss->cap *= 2;
	}
	head = &val->val.seq.list;
	diss->stack[diss->top ++] = head;
	diss->cursor = head;
	
	return true;
}

bool brpc_dissect_out(brpc_dissect_t *diss)
{
	brpc_val_t *val;
	/* pos 0 is keeping the basic sequence (message or value. */
	if (diss->top <= 1) { 
		WERRNO(ENOMSG);
		ERR("trying to pop emtpy stack");
		return false;
	}
	val = _BRPC_VAL4SEQ(diss->stack[-- diss->top]);
	diss->cursor = &val->list;
	return true;
}

bool brpc_dissect_next(brpc_dissect_t *diss)
{
	/* is current the last in list? */
	if (diss->cursor->next == diss->stack[diss->top - 1])
		return false;
	diss->cursor = diss->cursor->next;
	return true;
}

/* functionality moved into values tree */
size_t brpc_dissect_cnt(brpc_dissect_t *diss)
{
	struct brpc_list_head *pos, *head;
	size_t cnt;

	head = diss->stack[diss->top - 1];
	pos = head->next;
	if ((diss->type == BRPC_MSG_DISSECTOR) && 
			(diss->msg->type == BRPC_CALL_REQUEST) &&
			/* (diss->top == 1) &&   :we can skip this one,the next is enough*/
			(head == diss->stack[0 /*==top-1*/]))
			/* we're on level 0 */
			/* get rid of method's name value */
			pos = pos->next;
	for (cnt = 0; pos != head; pos = pos->next, cnt ++)
		;
	return cnt;
}

brpc_vtype_t brpc_dissect_seqtype(brpc_dissect_t *diss)
{
	ssize_t idx = diss->top - 1;
	if (! idx)
		return BRPC_VAL_NONE;
	return _BRPC_VAL4SEQ(diss->stack[idx])->type;
}

bool brpc_dissect_chain(brpc_dissect_t *anchor, brpc_dissect_t *cell)
{
	if (! list_empty(&cell->list)) {
		WERRNO(EINVAL);
		ERR("dissector already chained.\n");
		return false;
	}
	list_add(&cell->list, &anchor->head);
	return true;
}

size_t brpc_dissect_levcnt(brpc_dissect_t *diss)
{
	ssize_t idx = diss->top - 1;
	if ((diss->type == BRPC_MSG_DISSECTOR) && (! idx)) {
		/* must return count in message: dec method value */
		return brpc_val_cnt(diss->msg) - /* method counted down */1;
	} else {
		brpc_val_t *val = _BRPC_VAL4SEQ(diss->stack[idx]);
		return brpc_val_seqcnt(val);
	}
}
