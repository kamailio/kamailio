/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifndef _HOOKS_H
#define _HOOKS_H

#include "defs.h"

/* TMCB_ONSEND used to enable certain callback-related features when
 * ONSEND was set, these days it's always enabled. For compatibility
 * reasons with modules that check ONSEND, continue to set it
 * unconditionally*/
#define TMCB_ONSEND
#include "../../ip_addr.h" /* dest_info */

struct sip_msg;
struct cell;

#define TMCB_REQUEST_IN_N       0
#define TMCB_RESPONSE_IN_N      1
#define TMCB_E2EACK_IN_N        2
#define TMCB_REQUEST_PENDING_N  3
#define TMCB_REQUEST_FWDED_N    4
#define TMCB_RESPONSE_FWDED_N   5
#define TMCB_ON_FAILURE_RO_N    6
#define TMCB_ON_FAILURE_N       7
#define TMCB_REQUEST_OUT_N      8
#define TMCB_RESPONSE_OUT_N     9
#define TMCB_LOCAL_COMPLETED_N  10
#define TMCB_LOCAL_RESPONSE_OUT_N 11
#define TMCB_ACK_NEG_IN_N       12
#define TMCB_REQ_RETR_IN_N      13
#define TMCB_LOCAL_RESPONSE_IN_N 14
#define TMCB_LOCAL_REQUEST_IN_N  15
#define TMCB_DLG_N              16
#define TMCB_DESTROY_N          17  /* called on transaction destroy */
#define TMCB_E2ECANCEL_IN_N     18
#define TMCB_E2EACK_RETR_IN_N   19
#define TMCB_RESPONSE_READY_N	20
#ifdef WITH_AS_SUPPORT
#define TMCB_DONT_ACK_N         21 /* TM shoudn't ACK a local UAC  */
#endif
#define TMCB_REQUEST_SENT_N     22
#define TMCB_RESPONSE_SENT_N    23
#define TMCB_ON_BRANCH_FAILURE_RO_N 24
#define TMCB_ON_BRANCH_FAILURE_N 25
#define TMCB_MAX_N              25


#define TMCB_REQUEST_IN       (1<<TMCB_REQUEST_IN_N)
#define TMCB_RESPONSE_IN      (1<<TMCB_RESPONSE_IN_N)
#define TMCB_E2EACK_IN        (1<<TMCB_E2EACK_IN_N)
#define TMCB_REQUEST_PENDING  (1<<TMCB_REQUEST_PENDING_N)
#define TMCB_REQUEST_FWDED    (1<<TMCB_REQUEST_FWDED_N)
#define TMCB_RESPONSE_FWDED   (1<<TMCB_RESPONSE_FWDED_N)
#define TMCB_ON_FAILURE_RO    (1<<TMCB_ON_FAILURE_RO_N)
#define TMCB_ON_FAILURE       (1<<TMCB_ON_FAILURE_N)
#define TMCB_REQUEST_OUT      (1<<TMCB_REQUEST_OUT_N)
#define TMCB_RESPONSE_OUT     (1<<TMCB_RESPONSE_OUT_N)
#define TMCB_LOCAL_COMPLETED  (1<<TMCB_LOCAL_COMPLETED_N)
#define TMCB_LOCAL_RESPONSE_OUT (1<<TMCB_LOCAL_RESPONSE_OUT_N)
#define TMCB_ACK_NEG_IN       (1<<TMCB_ACK_NEG_IN_N)
#define TMCB_REQ_RETR_IN      (1<<TMCB_REQ_RETR_IN_N)
#define TMCB_LOCAL_RESPONSE_IN (1<<TMCB_LOCAL_RESPONSE_IN_N)
#define TMCB_LOCAL_REQUEST_IN (1<<TMCB_LOCAL_REQUEST_IN_N)
#define TMCB_DLG              (1<<TMCB_DLG_N)
#define TMCB_DESTROY          (1<<TMCB_DESTROY_N)
#define TMCB_E2ECANCEL_IN     (1<<TMCB_E2ECANCEL_IN_N)
#define TMCB_E2EACK_RETR_IN   (1<<TMCB_E2EACK_RETR_IN_N)
#define TMCB_RESPONSE_READY   (1<<TMCB_RESPONSE_READY_N)
#ifdef WITH_AS_SUPPORT
#define TMCB_DONT_ACK         (1<<TMCB_DONT_ACK_N)
#endif
#define TMCB_REQUEST_SENT      (1<<TMCB_REQUEST_SENT_N)
#define TMCB_RESPONSE_SENT     (1<<TMCB_RESPONSE_SENT_N)
#define TMCB_ON_BRANCH_FAILURE (1<<TMCB_ON_BRANCH_FAILURE_N)
#define TMCB_ON_BRANCH_FAILURE_RO (1<<TMCB_ON_BRANCH_FAILURE_RO_N)
#define TMCB_MAX              ((1<<(TMCB_MAX_N+1))-1)


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
 * Note: this callback MUST be installed before forking
 * (the req_in_tmcb_hl callback list does not live in shmem and has no access
 * protection), i.e., at best from mod_init functions.
 *
 * Note: All the other callbacks can be safely installed when the
 * transaction already exists, it does not need to be locked.
 * 
 * TMCB_RESPONSE_IN -- a brand-new reply was received which matches
 * an existing non-local transaction. It may or may not be a retransmission.
 * No lock is held here (yet).
 * Note: for an invite transaction this callback will also catch the reply
 *  to local cancels (e.g. branch canceled due to fr_inv_timeout). To
 *  distinguish between the two, one would need to look at the method in
 *  Cseq (look at t_reply.c:1630 (reply_received()) for an example).
 *
 *  TMCB_RESPONSE_OUT -- a final or provisional reply was sent out
 *  successfully (either a local reply  or a proxied one).
 *  For final replies is called only for the first one (it's not called
 *  for retransmissions).
 *  For non-local replies (proxied) is called also for provisional responses
 *  (NOTE: this might change and in the future it might be called only
 *  for final replies --andrei).
 *  For local replies is called _only_ for the final reply.
 *  There is nothing more you can change from the callback, it is good for 
 *  accounting-like uses. No lock is held.
 *  Known oddities: it's called for provisional replies for relayed replies,
 *  but not for local responses (see NOTE above).
 *  Note: if the send fails or via cannot be resolved, this callback is 
 *  _not_ called.
 *  Note: local reply means locally generated reply (via t_reply() & friends)
 *  and not local transaction.
 *
 *    Note: the message passed to the callback may also have
 *    value FAKED_REPLY (like other reply callbacks) which
 *    indicates a local reply caused by a timer, calling t_reply() a.s.o.
*     Check for this value before deferring -- you will cause a segfault
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
 *  a chance for example to add new transaction branches.
 *  WARNING: the REPLY lock is held.
 *  It is safe to add more callbacks from here.
 *
 *  TMCB_RESPONSE_FWDED -- called when a reply is about to be
 *  forwarded; it is called after a message is received but before
 *  a message is sent out: it is called when the decision is
 *  made to forward a reply; it is parametrized by pkg message
 *  which caused the transaction to complete (which is not
 *  necessarily the same which will be forwarded). As forwarding
 *  has not been executed and may fail, there is no guarantee
 *  a reply will be successfully sent out at this point of time.
 *
 *     Note: TMCB_ON_FAILURE and TMCB_REPLY_FWDED are
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
 *  transaction occurrence will be matched with spirals. If
 *  record-routing is not enabled, you will never receive the
 *  ACK and the callback will be never triggered. In general it's called only
 *   for the first ACK but it can be also called multiple times 
 *   quasi-simultaneously if multiple ACK copies arrive in parallel or if
 *   ACKs with different (never seen before) to-tags are received.
 *
 *   TMCB_E2EACK_RETR_IN -- like TMCB_E2EACK_IN, but matches retransmissions
 *   and it's called for every retransmission (but not for the "first" ACK).
 *
 *  TMCB_E2ECANCEL_IN -- called when a CANCEL for the INVITE transaction
 *  for which the callback was registered arrives.
 *   The transaction parameter will point to the invite transaction (and 
 *   not the cancel) and the request parameter to the CANCEL sip msg.
 *   Note: the callback should be registered for an INVITE transaction.
 *
 *  TMCB_REQUEST_FWDED -- request is being forwarded out. It is
 *  called before a message is forwarded, when the corresponding branch
 *   is created (it's called for each branch) and it is your last
 *  chance to change its shape. It can also be called from the failure
 *   router (via t_relay/t_forward_nonack) and in this case the REPLY lock 
 *   will be held.
 *
 *  TMCB_REQUEST_OUT -- request was sent out successfully.
 *  There is nothing more you can change from the callback, it is good for
 *  accounting-like uses.
 *  Note: if the send fails or via cannot be resolved, this callback is
 *  _not_ called.
 *
 *  TMCB_LOCAL_COMPLETED -- final reply for localy initiated
 *  transaction arrived. Message may be FAKED_REPLY. Can be called multiple
 *  times, no lock is held.
 *
 *  TMCB_LOCAL_RESPONSE_OUT -- provisional reply for localy initiated 
 *  transaction. The message may be a FAKED_REPLY and the callback might be 
 *  called multiple time quasi-simultaneously. No lock is held.
 *  Note: depends on tm.pass_provisional_replies.
 *  Note: the name is very unfortunate and it will probably be changed
 *   (e.g. TMCB_LOCAL_PROVISIONAL).
 *
 *  TMCB_NEG_ACK_IN -- an ACK to a negative reply was received, thus ending
 *  the transaction (this happens only when the final reply sent by tm is 
 *  negative). The callback might be called simultaneously. No lock is held.
 *
 *  TMCB_REQ_RETR_IN -- a retransmitted request was received. This callback
 *   might be called simultaneously. No lock is held.
 *
 * TMCB_LOCAL_RESPONSE_IN -- a brand-new reply was received which matches
 * an existing local transaction (like TMCB_RESPONSE_IN but for local 
 * transactions). It may or may not be a retransmission.
 *
 * TMCB_LOCAL_REQUEST_IN -- like TMCB_REQUEST_IN but for locally generated 
 * request (e.g. via fifo/rpc):  a brand-new local request was 
 * received/generated and a transaction for it is about to be created.
 * It's called from HASH_LOCK, so be careful. It is guaranteed not to be
 * a retransmission. The transactional context is mostly
 * incomplete -- this callback is called in very early stage
 * before the message is shmem-ized (so that you can work
 * with it).
 * It's safe to install other TMCB callbacks from here.
 * Note: this callback MUST be installed before forking
 * (the local_req_in_tmcb_hl callback list does not live in shmem and has no 
 * access protection), i.e., at best from mod_init functions.
 *
 *
 *  All of the following callbacks are called immediately after or before 
 *  sending a message. All of them are read-only (no change can be made to
 * the message). These callbacks use the t_rbuf, send_buf, dst, is_retr
 *  and the code members of the tmcb_params structure.
 *  For a request code is <=0. code values can be TYPE_LOCAL_ACK for an ACK 
 *  generated by ser, TYPE_LOCAL_CANCEL for a CANCEL generated by ser 
 *  and TYPE_REQUEST for all the other requests or requests generated via 
 *  t_uac.
 *   For a reply the code is the response status (which is always >0, e.g. 200,
 *   408, a.s.o).
 *        - the callbacks will be called sometimes with the REPLY lock held
 *          and sometimes without it, so trying to acquire the REPLY lock
 *          from these callbacks could lead to deadlocks (avoid it unless
 *           you really know what you're doing).
 *
 *  TMCB_REQUEST_SENT -- called each time a request was sent (even for
 *  retransmissions), it includes local and forwarded request, ser generated
 *  CANCELs and ACKs. The tmcb_params structure will have the t_rbuf, dst,
 *  send_buf and is_retr members filled.
 *  This callback is "read-only", the message was already sent and no changes
 *  are allowed.
 *  Note: send_buf can be different from t_rbuf->buffer for ACKs (in this
 *   case t_rbuf->buf will contain the last request sent on the branch and
 *   its destination). The same goes for t_rbuf->dst and tmcb->dst for local 
 *   transactions ACKs to 2xxs.
 *
 *  TMCB_RESPONSE_SENT -- called each time a response was sent (even for
 *  retransmissions). The tmcb_params structure will have t_rbuf set to the
 *  reply retransmission buffer and send_buf set to the data sent (in this case
 *  it will always be the same with t_rbuf->buf). is_retr will also be set if
 *  the reply is retransmitted
 *   by ser.
 *  This callback is "read-only", the message was already sent and no changes
 *  are allowed.
 *
 *  TMCB_DESTROY -- called when the transaction is destroyed. Everything but
 *  the cell* parameter (t) and the tmcb are set to 0. Only the param is
 *  is filled inside TMCB. For dialogs callbacks t is also 0.
 *
 * TMCB_RESPONSE_READY -- a reply is ready to be sent out. Callback is
 *  is executed just before writing the reply content to network.
 *
 * TMCB_DONT_ACK (requires AS support) -- for localy generated INVITEs, TM 
 * automatically generates an ACK for the received 2xx replies. But, if this 
 * flag is passed to TM when creating the initial UAC request, this won't
 * happen anymore: the ACK generation must be triggered from outside, using
 * TM's interface.
 * While this isn't exactly a callback type, it is used as part of the flags
 * mask when registering callbacks.

	the callback's param MUST be in shared memory and will
	NOT be freed by TM; you must do it yourself from the
	callback function if necessary (for example register it also for 
	 TMCB_DESTROY and when called with TMCB_DESTROY just free the param
	).
*/

#define TMCB_RETR_F 1
#define TMCB_LOCAL_F 2

/* pack structure with all params passed to callback function */
struct tmcb_params {
	struct sip_msg* req;
	struct sip_msg* rpl;
	void **param;
	int code;
	unsigned short flags; /* set to a combination of:
							 TMCB_RETR_F if this is a _ser_ retransmission
							 (but not if if it's a "forwarded" retr., like a 
							 retr. 200 Ok for example)
							 TMCB_LOCAL_F if this is a local generated message
							  (and not forwarded) */
	unsigned short branch;
	/* could also be: send_buf, dst, branch */
	struct retr_buf* t_rbuf; /* transaction retr. buf., all the information
								 regarding destination, data that is/was
								 actually sent on the net, branch a.s.o is
								 inside */
	struct dest_info* dst; /* destination */
	str send_buf; /* what was/will be sent on the net, used for ACKs
					(which don't have a retr_buf). */
};

#define INIT_TMCB_PARAMS(tmcb, request, reply, r_code)\
do{\
	memset(&(tmcb), 0, sizeof((tmcb))); \
	(tmcb).req=(request); (tmcb).rpl=(reply);  \
	(tmcb).code=(r_code); \
}while(0)

#define INIT_TMCB_ONSEND_PARAMS(tmcb, req, repl, rbuf, dest, buf, buf_len, \
								onsend_flags, t_branch, code) \
do{ \
	INIT_TMCB_PARAMS(tmcb, req, repl, code); \
	tmcb.t_rbuf=(rbuf); tmcb.dst=(dest); \
	tmcb.send_buf.s=(buf); tmcb.send_buf.len=(buf_len); \
	tmcb.flags=(onsend_flags); tmcb.branch=(t_branch); \
}while(0)

/* callback function prototype */
typedef void (transaction_cb) (struct cell* t, int type, struct tmcb_params*);
/*! \brief function to release the callback param */
typedef void (release_tmcb_param) (void* param);
/* register callback function prototype */
typedef int (*register_tmcb_f)(struct sip_msg* p_msg, struct cell *t,
							   int cb_types, transaction_cb f, void *param,
							   release_tmcb_param func);


struct tm_callback {
	int id;                      /* id of this callback - useless */
	int types;                   /* types of events that trigger the callback*/
	transaction_cb* callback;    /* callback function */
	void *param;                 /* param to be passed to callback function */
	release_tmcb_param* release; /**< Function to release the callback param
								  * when the callback is deleted */
	struct tm_callback* next;
};

struct tmcb_head_list {
	struct tm_callback volatile *first;
	int reg_types;
};


extern struct tmcb_head_list*  req_in_tmcb_hl;
extern struct tmcb_head_list*  local_req_in_tmcb_hl;

void set_early_tmcb_list(struct sip_msg *msg,
		struct cell *t);

#define has_tran_tmcbs(_T_, _types_) \
	( ((_T_)->tmcb_hl.reg_types)&(_types_) )
#define has_reqin_tmcbs() \
	( req_in_tmcb_hl->first!=0 )
#define has_local_reqin_tmcbs() \
	( local_req_in_tmcb_hl->first!=0 )


int init_tmcb_lists(void);

void destroy_tmcb_lists(void);


/* register a callback for several types of events */
int register_tmcb( struct sip_msg* p_msg, struct cell *t, int types,
				   transaction_cb f, void *param,
				   release_tmcb_param rel_func);

/* inserts a callback into the a callback list */
int insert_tmcb(struct tmcb_head_list *cb_list, int types,
				transaction_cb f, void *param,
				release_tmcb_param rel_func);

/* run all transaction callbacks for an event type */
void run_trans_callbacks( int type , struct cell *trans,
						struct sip_msg *req, struct sip_msg *rpl, int code );
/* helper function */
void run_trans_callbacks_internal(struct tmcb_head_list* cb_lst, int type,
									struct cell *trans, 
									struct tmcb_params *params);
/* run all REQUEST_IN callbacks */
void run_reqin_callbacks( struct cell *trans, struct sip_msg *req, int code );
void run_local_reqin_callbacks( struct cell *trans, struct sip_msg *req, 
		int code );

/* like run_trans_callbacks but provide outgoing buffer (i.e., the
 * processed message) to callback */
void run_trans_callbacks_with_buf(int type, struct retr_buf* rbuf, struct sip_msg* req,
								  struct sip_msg* repl, short flags);

/* like run_trans_callbacks but tmcb_params assumed to contain data already */
void run_trans_callbacks_off_params(int type, struct cell* t, struct tmcb_params* p);

#endif
