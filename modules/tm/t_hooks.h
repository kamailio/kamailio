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
 * 2003-03-16 : backwards-compatibility callback names introduced (jiri)
 * 2003-03-06 : old callbacks renamed, new one introduced (jiri)
 * 2003-12-04 : global callbacks moved into transaction callbacks;
 *              multiple events per callback added; single list per
 *              transaction for all its callbacks (bogdan)
 */


#ifndef _HOOKS_H
#define _HOOKS_H

#include "defs.h"

struct sip_msg;
struct cell;


#define TMCB_REQUEST_IN       (1<<0)
#define TMCB_RESPONSE_IN      (1<<1)
#define TMCB_E2EACK_IN        (1<<2)
#define TMCB_REQUEST_FWDED    (1<<3)
#define TMCB_RESPONSE_FWDED   (1<<4)
#define TMCB_ON_FAILURE_RO    (1<<5)
#define TMCB_ON_FAILURE       (1<<6)
#define TMCB_RESPONSE_OUT     (1<<7)
#define TMCB_LOCAL_COMPLETED  (1<<8)
#define TMCB_MAX              ((1<<9)-1)

/* 
 *  Caution: most of the callbacks work with shmem-ized messages
 *  which you can no more change (e.g., lumps are fixed). Most
 *  reply-processing callbacks are also called from a mutex,
 *  which may cause deadlock if you are not careful. Also, reply
 *  callbacks may pass the value of FAKED_REPLY messages, which
 *  is a non-dereferencable pointer indicating that no message
 *  was received and a timer hit instead.
 *
 *  All callbacks excepting the TMCB_REQUEST_IN are associates to a
 *  transaction. It means they will be run only when the event will hint
 *  the transaction the callbacks were register for.
 *  TMCB_REQUEST_IN is a global callback - it means it will be run for
 *  all transactions.
 *
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
 *  TMCB_ON_FAILURE_RO -- called on receipt of a reply or timer;
 *  it means all branches completed with a failure; the callback 
 *  function MUST not change anything in the transaction (READONLY)
 *  that's a chance for doing ACC or stuff like this
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

	note that callbacks MUST be installed before forking
	(callback lists do not live in shmem and have no access
	protection), i.e., at best from mod_init functions.

	the callback's param MUST be in shared memory and will
	NOT be freed by TM; you must do it yourself from the
	callback function id necessary.
*/


/* pack structure with all params passed toa callback function */
struct tmcb_params {
	struct sip_msg* req;
	struct sip_msg* rpl;
	int code;
	void *param;
};

/* callback function prototype */
typedef void (transaction_cb) (struct cell* t, int type, struct tmcb_params*);
/* register callback function prototype */
typedef int (*register_tmcb_f)(struct sip_msg* p_msg, int cb_types,
		transaction_cb f, void *param);


struct tm_callback {
	int id;                      /* id of this callback - useless */
	int types;                   /* types of events that trigger the callback*/
	transaction_cb* callback;    /* callback function */
	void *param;                 /* param to be passed to callback function */
	struct tm_callback* next;
};

struct tmcb_head_list {
	struct tm_callback *first;
	int reg_types;
};


extern struct tmcb_head_list*  req_in_tmcb_hl;


#define has_tran_tmcbs(_T_, _types_) \
	( ((_T_)->tmcb_hl.reg_types)|(_types_) )
#define has_reqin_tmcbs() \
	( req_in_tmcb_hl->first!=0 )


int init_tmcb_lists();

void destroy_tmcb_lists();


/* register a callback for several types of events */
int register_tmcb( struct sip_msg* p_msg, int types, transaction_cb f,
																void *param );

/* inserts a callback into the a callback list */
int insert_tmcb(struct tmcb_head_list *cb_list, int types,
									transaction_cb f, void *param );

/* run all transaction callbacks for an event type */
void run_trans_callbacks( int type , struct cell *trans,
						struct sip_msg *req, struct sip_msg *rpl, int code );

/* run all REQUEST_IN callbacks */
void run_reqin_callbacks( struct cell *trans, struct sip_msg *req, int code );

#endif
