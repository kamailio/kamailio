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

#ifndef __BINRPC_CB_H__
#define __BINRPC_CB_H__

#include "call.h"


/**
 * API.
 */

/**
 * Call back type for replies.
 * @param rpl Reply for request; may also be failure indicator.
 * @param opaque Reference registered along with the callback.
 */
typedef void (*brpc_cb_rpl_f)(brpc_t *rpl, void *opaque);
/**
 * Call back type for requests.
 * @param req The BINRPC request.
 * @param opaque Reference registered along with the callback.
 * @return The callback returns a reply to the request.
 */
typedef brpc_t *(*brpc_cb_req_f)(brpc_t *req, void *opaque);

/**
 * Initialize callback subsystem.
 * @param reqsz HT size for requests. Disabled if 0.
 * @param rplsz HT size for replies. Disabled if 0.
 * @return Status of operation.
 */
bool brpc_cb_init(size_t reqsz, size_t rplsz);

/**
 * Releases all allocated resources.
 */
void brpc_cb_close(void);

/**
 * Register a callback to process the reply to be received for a request.
 * @param req Request for which a reply is expected.
 * @param callback The function to be called when the reply is received.
 * @param opaque Value to be passed to the callback along with the reply.
 * @return Status of operation.
 *
 * NOTE: 
 * 1. there's no timer to expire and to fire the callback. @see 
 * brpc_cb_rpl_cancel.
 * 2. call's ID is used to match requests with replies: temporary uniqueness
 * must be ensured.
 */
bool brpc_cb_rpl(brpc_t *req, brpc_cb_rpl_f callback, void *opaque);
/**
 * Force the callback associated with this request, with first parameter (the
 * reply) equal to NULL; the registration's also removed.
 * @param req The reference passed when registering the callback.
 * @return True, if callback invoked; false if registration not available
 * anymore (which can happen in multithread context).
 */
bool brpc_cb_rpl_cancel(brpc_t *req);
/**
 * Register a serving function.
 * @param name The RPC's name.
 * @param sign Signature of the call; can be emtpy (but not NULL).
 * @param callback The actual implementation.
 * @param doc Description of the call; can be NULL.
 * @opaque Reference passed back when calling the callback.
 * @return Status of operation.
 */
bool brpc_cb_req(char *name, char *sign, brpc_cb_req_f callback, char *doc, 
		void *opaque);
/**
 * Remove a previously registered callback.
 * @param name The name the callback was registered under.
 * @param sign Call's signature;
 * @return Operation status.
 */
bool brpc_cb_req_rem(char *name, char *sign);

/**
 * Dispatches a BINRPC call (request or reply).
 * @param call BINRPC call.
 * @return A BINRPC reply, if it's the case. NOTE: its impossible to know
 * whether a reply couldn't be generated (due to some failure) or it wasn't
 * the case. If distinction needed, use brpc_errno or special values
 * ((brpc_t *)LARGE_NEGATIVE)).
 */
brpc_t *brpc_cb_run(brpc_t *call);

#ifdef _LIBBINRPC_BUILD

#include "ht.h"
#include "value.h"

typedef struct {
	ht_lnk_t lnk;
	brpc_str_t name;
	brpc_str_t sign;
	brpc_cb_req_f cb_fn;
	bool deepchk; /* do argument matching, besides name matching */
	brpc_str_t doc;
	void *opaque;
} req_cb_t;

typedef struct {
	ht_lnk_t lnk;
	brpc_id_t cid; /* call ID */
	brpc_cb_rpl_f cb_fn;
	void *opaque;
} rpl_cb_t;

#endif /* _LIBBINRPC_BUILD */

#endif /* __BINRPC_CB_H__ */
