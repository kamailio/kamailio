/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2003-03-16 backwards-compatibility callback names introduced (jiri)
 * 2003-03-06 old callbacks renamed, new one introduced (jiri)
 */


#ifndef _HOOKS_H
#define _HOOKS_H

#include "defs.h"


struct sip_msg;
struct cell;

/* backwards compatibility hooks */
#define TMCB_REPLY TMCB_RESPONSE_OUT
#define TMCB_E2EACK TMCB_E2EACK_IN
#define TMCB_REPLY_IN TMCB_RESPONSE_IN
#define TMCB_REQUEST_OUT TMCB_REQUEST_FWDED
#define TMCB_ON_NEGATIVE_TMCB_ON_FAILURE

typedef enum { 
		/* input events */
		TMCB_RESPONSE_IN=1, TMCB_REQUEST_IN, TMCB_E2EACK_IN, 
		/* routing decisions in progress */
		TMCB_REQUEST_FWDED, TMCB_RESPONSE_FWDED, TMCB_ON_FAILURE,
		/* completion events */
		TMCB_RESPONSE_OUT, TMCB_LOCAL_COMPLETED, 
		TMCB_END } tmcb_type;

/* 
 *  Caution: most of the callbacks work with shmem-ized messages
 *  which you can no more change (e.g., lumps are fixed). Most
 *  reply-processing callbacks are also called from a mutex,
 *  which may cause deadlock if you are not careful. Also, reply
 *  callbacks may pass the value of FAKED_REPLY messages, which
 *  is a non-dereferencable pointer indicating that no message
 *  was received and a timer hit instead.
 *
 *  Callback description:
 *  ---------------------
 *
 * TMCB_REQUEST_IN -- a brand-new request was received and is
 * about to establish transaction; it is not yet cloned and
 * lives in pkg mem -- your last chance to mangle it before
 * it gets shmem-ized (then, it's read-only); it's called from
 * HASH_LOCK, so be careful. It is guaranteed not to be
 * a retransmission. The transactional context is mostly
 * incomplete -- this callback is called in very early stage
 * before the message is shmem-ized (so that you can work
 * with it).
 *
 * TMCB_RESPONSE_IN -- a brand-new reply was received which matches
 * an existing transaction. It may or may not be a retranmisssion.
 *
 *  TMCB_RESPONSE_OUT -- a final reply was sent out (eiter local 
 *  or proxied) -- there is nothing more you can change from
 *  the callback, it is good for accounting-like uses.
 *
 *    Note: the message passed to callback may also have
 *    value FAKED_REPLY (like other reply callbacks) which
 *    indicates a psedo_reply caused by a timer. Check for
 *    this value before derefing -- you will cause a segfault
 *    otherwise. Check for t->uas.request validity too if you
 *    need it ... locally initiated UAC transactions set it to 0.
 *
 *    Also note, that reply callbacks are not called if a transaction
 *    is dropped silently. That's the case when noisy_ctimer is
 *    disabled (by default) and C-timer hits. The proxy server then
 *    drops state silently, doesn't use callbacks and expects the
 *    transaction to complete statelessly.
 *
 *  TMCB_ON_FAILURE -- called on receipt of a reply or timer;
 *  it means all branches completed with a failure; that's 
 *  a chance for example to add new transaction branches
 *
 *  TMCB_RESPONSE_FWDED -- called when a reply is about to be
 *  forwarded; it is called after a message is received but before
 *  a message is sent out: it is called when the decision is 
 *  made to forward a reply; it is parametrized by pkg message 
 *  which caused the transaction to complete (which is not 
 *  necessarily the same which will be forwarded). As forwarding
 *  has not been executed and may fail, there is no guarentee
 *  a reply will be successfuly sent out at this point of time.
 *
 *     Note: TMCB_REPLY_ON_FAILURE and TMCB_REPLY_FWDED are
 *     called from reply mutex which is used to deterministically
 *     process multiple replies received in parallel. A failure
 *     to set the mutex again or stay too long in the callback
 *     may result in deadlock.
 *
 *     Note: the reply callbacks will not be evoked if "silent
 *     C-timer hits". That's a feature to clean transactional
 *     state from a proxy quickly -- transactions will then
 *     complete statelessly. If you wish to disable this
 *     feature, either set the global option "noisy_ctimer"
 *     to 1, or set t->noisy_ctimer for selected transaction.
 *
 *  TMCB_E2EACK_IN -- called when an ACK belonging to a proxied
 *  INVITE transaction completed with 200 arrived. Note that
 *  because it can be only dialog-wise matched, only the first
 *  transaction occurence will be matched with spirals. If
 *  record-routing is not enabled, you will never receive the
 *  ACK and the callback will be never triggered.
 *
 *
 *  TMCB_REQUEST_FWDED -- request is being forwarded out. It is 
 *  called before a message is forwarded and it is your last
 *  chance to change its shape. 
 *
 *  TMCB_LOCAL COMPLETED -- final reply for localy initiated
 *  transaction arrived. Message may be FAKED_REPLY.
 *
 *  TMCB_END	- just a bumper

	see the 'acc' module for an example of callback usage

	note that callbacks MUST be installed before forking
    (callback lists do not live in shmem and have no access
	protection), i.e., at best from mod_init functions.

	also, note: the callback param is currently not used;
	if whoever wishes to use a callback parameter, use
	trans->cbp
*/

typedef void (transaction_cb) ( struct cell* t, struct sip_msg* msg, 
	int code, void *param );

struct tm_callback_s {
	int id;
	transaction_cb* callback;
	struct tm_callback_s* next;
	void *param;
};


extern struct tm_callback_s* callback_array[ TMCB_END ];

typedef int (*register_tmcb_f)(tmcb_type cbt, transaction_cb f, void *param);

int register_tmcb( tmcb_type cbt, transaction_cb f, void *param );
void callback_event( tmcb_type cbt, struct cell *trans,
	struct sip_msg *msg, int code );

#endif
