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

#include <stdio.h> /* for FILE* in fifo_uac_cancel */
#ifdef EXTRA_DEBUG
#include <assert.h>
#endif /* EXTRA_DEBUG */

#include "defs.h"
#include "config.h"

#include "t_funcs.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "t_reply.h"
#include "t_cancel.h"
#include "t_msgbuilder.h"
#include "t_lookup.h" /* for t_lookup_callid in fifo_uac_cancel */
#include "t_hooks.h"


/** Prepare to cancel a transaction.
 * Determine which branches should be canceled and prepare them (internally
 * mark them as "cancel in progress", see prepare_cancel_branch()).
 * Can be called without REPLY_LOCK, since prepare_cancel_branch() is atomic 
 *  now *  -- andrei
 * WARNING: - has side effects, see prepare_cancel_branch()
 *          - one _must_ call cancel_uacs(cancel_bm) if *cancel_bm!=0 or
 *             you'll have some un-cancelable branches (because they remain
 *             "marked" internally)
 * @param t - transaction whose branches will be canceled
 * @param cancel_bm - pointer to a branch bitmap that will be filled with
*    the branches that must be canceled (must be passed to cancel_uacs() if
*    !=0).
*  @param skip - branch bitmap of branches that should not be canceled
*/
void prepare_to_cancel(struct cell *t, branch_bm_t *cancel_bm,
						branch_bm_t skip_branches)
{
	int i;
	int branches_no;
	branch_bm_t mask;
	
	*cancel_bm=0;
	branches_no=t->nr_of_outgoings;
	mask=~skip_branches;
	membar_depends(); 
	for( i=0 ; i<branches_no ; i++ ) {
		*cancel_bm |= ((mask & (1<<i)) &&  prepare_cancel_branch(t, i, 1))<<i;
	}
}




/* cancel branches scheduled for deletion
 * params: t          - transaction
 *          cancel_data - structure filled with the cancel bitmap (bitmap with
 *                       the branches that are supposed to be canceled) and
 *                       the cancel reason.
 *          flags     - how_to_cancel flags, see cancel_branch()
 * returns: bitmap with the still active branches (on fr timer)
 * WARNING: always fill cancel_data->cancel_bitmap using prepare_to_cancel(),
 *          supplying values in any other way is a bug*/
int cancel_uacs( struct cell *t, struct cancel_info* cancel_data, int flags)
{
	int i;
	int ret;
	int r;

	ret=0;
	/* cancel pending client transactions, if any */
	for( i=0 ; i<t->nr_of_outgoings ; i++ ) 
		if (cancel_data->cancel_bitmap & (1<<i)){
			r=cancel_branch(
				t,
				i,
#ifdef CANCEL_REASON_SUPPORT
				&cancel_data->reason,
#endif /* CANCEL_REASON_SUPPORT */
				flags | ((t->uac[i].request.buffer==NULL)?
					F_CANCEL_B_FAKE_REPLY:0) /* blind UAC? */
			);
			ret|=(r!=0)<<i;
		}
	return ret;
}

int cancel_all_uacs(struct cell *trans, int how)
{
	struct cancel_info cancel_data;
	int i,j;

#ifdef EXTRA_DEBUG
	assert(trans);
#endif
	DBG("Canceling T@%p [%u:%u]\n", trans, trans->hash_index, trans->label);
	
	init_cancel_info(&cancel_data);
	prepare_to_cancel(trans, &cancel_data.cancel_bitmap, 0);
	 /* tell tm to cancel the call */
	i=cancel_uacs(trans, &cancel_data, how);
	
	if (how & F_CANCEL_UNREF)
#ifndef TM_DEL_UNREF
	/* in case of 'too many' _buggy_ invocations, the ref count (a uint) might 
	 * actually wrap around, possibly leaving the T leaking. */
#warning "use of F_CANCEL_UNREF flag is unsafe without defining TM_DEL_UNREF"
#endif
		UNREF(trans);

	/* count the still active branches */
	if (! how) {
		j=0;
		while(i){
			j++;
			i&=i-1;
		}
		return j;
	}
	return 0;
}


/* should be called directly only if one of the condition bellow is true:
 *  - prepare_cancel_branch or prepare_to_cancel returned true for this branch
 *  - buffer value was 0 and then set to BUSY in an atomic op.:
 *     if (atomic_cmpxchg_long(&buffer, 0, BUSY_BUFFER)==0).
 *
 * params:  t - transaction
 *          branch - branch number to be canceled
 *          reason - cancel reason structure
 *          flags - howto cancel: 
 *                   F_CANCEL_B_KILL - will completely stop the 
 *                     branch (stops the timers), use with care
 *                   F_CANCEL_B_FAKE_REPLY - will send a fake 487
 *                      to all branches that haven't received any response
 *                      (>=100). It assumes the REPLY_LOCK is not held
 *                      (if it is => deadlock)
 *                  F_CANCEL_B_FORCE_C - will send a cancel (and create the 
 *                       corresp. local cancel rb) even if no reply was 
 *                       received; F_CANCEL_B_FAKE_REPLY will be ignored.
 *                  F_CANCEL_B_FORCE_RETR - don't stop retransmission if no 
 *                       reply was received on the branch; incompatible
 *                       with F_CANCEL_B_FAKE_REPLY, F_CANCEL_B_FORCE_C and
 *                       F_CANCEL_B_KILL (all of them take precedence) a
 *                  default: stop only the retransmissions for the branch
 *                      and leave it to timeout if it doesn't receive any
 *                      response to the CANCEL
 * returns: 0 - branch inactive after running cancel_branch() 
 *          1 - branch still active  (fr_timer)
 *         -1 - error
 * WARNING:
 *          - F_CANCEL_B_KILL should be used only if the transaction is killed
 *            explicitly afterwards (since it might kill all the timers
 *            the transaction won't be able to "kill" itself => if not
 *            explicitly "put_on_wait" it might live forever)
 *          - F_CANCEL_B_FAKE_REPLY must be used only if the REPLY_LOCK is not
 *            held
 *          - checking for buffer==0 under REPLY_LOCK is no enough, an 
 *           atomic_cmpxhcg or atomic_get_and_set _must_ be used.
 */
int cancel_branch( struct cell *t, int branch,
	#ifdef CANCEL_REASON_SUPPORT
					struct cancel_reason* reason,
	#endif /* CANCEL_REASON_SUPPORT */
					int flags )
{
	char *cancel;
	unsigned int len;
	struct retr_buf *crb, *irb;
	int ret;
	struct cancel_info tmp_cd;
	void* pcbuf;

	crb=&t->uac[branch].local_cancel;
	irb=&t->uac[branch].request;
	irb->flags|=F_RB_CANCELED;
	ret=1;
	init_cancel_info(&tmp_cd);

#	ifdef EXTRA_DEBUG
	if (crb->buffer!=BUSY_BUFFER) {
		LOG(L_CRIT, "ERROR: attempt to rewrite cancel buffer: %p\n",
				crb->buffer);
		abort();
	}
#	endif

	if (flags & F_CANCEL_B_KILL){
		stop_rb_timers( irb );
		ret=0;
		if ((t->uac[branch].last_received < 100) &&
				!(flags & F_CANCEL_B_FORCE_C)) {
			DBG("DEBUG: cancel_branch: no response ever received: "
			    "giving up on cancel\n");
			/* remove BUSY_BUFFER -- mark cancel buffer as not used */
			pcbuf=&crb->buffer; /* workaround for type punning warnings */
			atomic_set_long(pcbuf, 0);
			/* try to relay auto-generated 487 canceling response only when
			 * another one is not under relaying on the branch and there is
			 * no forced response per transaction from script */
			if((flags & F_CANCEL_B_FAKE_REPLY)
					&& !(irb->flags&F_RB_RELAYREPLY)
					&& !(t->flags&T_ADMIN_REPLY)) {
				LOCK_REPLIES(t);
				if (relay_reply(t, FAKED_REPLY, branch, 487, &tmp_cd, 1) == 
										RPS_ERROR){
					return -1;
				}
			}
			/* do nothing, hope that the caller will clean up */
			return ret;
		}
	}else{
		if (t->uac[branch].last_received < 100){
			if (!(flags & F_CANCEL_B_FORCE_C)) {
				/* no response received => don't send a cancel on this branch,
				 *  just drop it */
				if (!(flags & F_CANCEL_B_FORCE_RETR))
					stop_rb_retr(irb); /* stop retransmissions */
				/* remove BUSY_BUFFER -- mark cancel buffer as not used */
				pcbuf=&crb->buffer; /* workaround for type punning warnings */
				atomic_set_long(pcbuf, 0);
				if (flags & F_CANCEL_B_FAKE_REPLY){
					stop_rb_timers( irb ); /* stop even the fr timer */
					LOCK_REPLIES(t);
					if (relay_reply(t, FAKED_REPLY, branch, 487, &tmp_cd, 1)== 
											RPS_ERROR){
						return -1;
					}
					return 0; /* should be inactive after the 487 */
				}
				/* do nothing, just wait for the final timeout */
				return 1;
			}
		}
		stop_rb_retr(irb); /* stop retransmissions */
	}

	if (cfg_get(tm, tm_cfg, reparse_invite) ||
			(t->uas.request && t->uas.request->msg_flags&(FL_USE_UAC_FROM|FL_USE_UAC_TO))) {
		/* build the CANCEL from the INVITE which was sent out */
		cancel = build_local_reparse(t, branch, &len, CANCEL, CANCEL_LEN,
									 (t->uas.request && t->uas.request->msg_flags&FL_USE_UAC_TO)?0:&t->to
	#ifdef CANCEL_REASON_SUPPORT
									 , reason
	#endif /* CANCEL_REASON_SUPPORT */
									 );
	} else {
		/* build the CANCEL from the received INVITE */
		cancel = build_local(t, branch, &len, CANCEL, CANCEL_LEN, &t->to
	#ifdef CANCEL_REASON_SUPPORT
								, reason
	#endif /* CANCEL_REASON_SUPPORT */
								);
	}
	if (!cancel) {
		LOG(L_ERR, "ERROR: attempt to build a CANCEL failed\n");
		/* remove BUSY_BUFFER -- mark cancel buffer as not used */
		pcbuf=&crb->buffer; /* workaround for type punning warnings */
		atomic_set_long(pcbuf, 0);
		return -1;
	}
	/* install cancel now */
	crb->dst = irb->dst;
	crb->branch = branch;
	/* label it as cancel so that FR timer can better know how to
	   deal with it */
	crb->activ_type = TYPE_LOCAL_CANCEL;
	/* be extra carefully and check for bugs (the below if could be replaced
	 *  by an atomic_set((void*)&crb->buffer, cancel) */
	if (unlikely(atomic_cmpxchg_long((void*)&crb->buffer, (long)BUSY_BUFFER,
							(long)cancel)!= (long)BUSY_BUFFER)){
		BUG("tm: cancel_branch: local_cancel buffer=%p != BUSY_BUFFER"
				" (trying to continue)\n", crb->buffer);
		shm_free(cancel);
		return -1;
	}
	membar_write_atomic_op(); /* cancel retr. can be called from 
								 reply_received w/o the reply lock held => 
								 they check for buffer_len to 
								 see if a valid reply exists */
	crb->buffer_len = len;

	DBG("DEBUG: cancel_branch: sending cancel...\n");
	if (SEND_BUFFER( crb )>=0){
		if (unlikely (has_tran_tmcbs(t, TMCB_REQUEST_OUT)))
			run_trans_callbacks_with_buf(TMCB_REQUEST_OUT, crb, t->uas.request, 0, TMCB_LOCAL_F);
		if (unlikely (has_tran_tmcbs(t, TMCB_REQUEST_SENT)))
			run_trans_callbacks_with_buf(TMCB_REQUEST_SENT, crb, t->uas.request, 0, TMCB_LOCAL_F);
	}
	/*sets and starts the FINAL RESPONSE timer */
	if (start_retr( crb )!=0)
		LOG(L_CRIT, "BUG: cancel_branch: failed to start retransmission"
					" for %p\n", crb);
	return ret;
}


/* fifo command to cancel a pending call (Uli)
 * Syntax:
 *
 * ":uac_cancel:[response file]\n
 * callid\n
 * cseq\n
 */
void rpc_cancel(rpc_t* rpc, void* c)
{
	struct cell *trans;
	static char cseq[128], callid[128];
	struct cancel_info cancel_data;
	int i,j;

	str cseq_s;   /* cseq */
	str callid_s; /* callid */

	cseq_s.s=cseq;
	callid_s.s=callid;
	init_cancel_info(&cancel_data);

	if (rpc->scan(c, "SS", &callid_s, &cseq_s) < 2) {
		rpc->fault(c, 400, "Callid and CSeq expected as parameters");
		return;
	}

	if( t_lookup_callid(&trans, callid_s, cseq_s) < 0 ) {
		DBG("Lookup failed\n");
		rpc->fault(c, 400, "Transaction not found");
		return;
	}
	/*  find the branches that need cancel-ing */
	prepare_to_cancel(trans, &cancel_data.cancel_bitmap, 0);
	 /* tell tm to cancel the call */
	DBG("Now calling cancel_uacs\n");
	i=cancel_uacs(trans, &cancel_data, 0); /* don't fake 487s, 
										 just wait for timeout */
	
	/* t_lookup_callid REF`d the transaction for us, we must UNREF here! */
	UNREF(trans);
	j=0;
	while(i){
		j++;
		i&=i-1;
	}
	rpc->add(c, "ds", j, "branches remaining (waiting for timeout)");
}



/* returns <0 on error */
int cancel_b_flags_get(unsigned int* f, int m)
{
	int ret;
	
	ret=0;
	switch(m){
		case 1:
			*f=F_CANCEL_B_FORCE_RETR;
			break;
		case 0:
			*f=F_CANCEL_B_FAKE_REPLY;
			break;
		case 2:
			*f=F_CANCEL_B_FORCE_C;
			break;
		default:
			*f=F_CANCEL_B_FAKE_REPLY;
			ret=-1;
	}
	return ret;
}



/* fixup function for the default cancel branch method/flags
 * (called by the configuration framework) */
int cancel_b_flags_fixup(void* handle, str* gname, str* name, void** val)
{
	unsigned int m,f;
	int ret;
	
	m=(unsigned int)(long)(*val);
	ret=cancel_b_flags_get(&f, m);
	if (ret<0)
		ERR("cancel_b_flags_fixup: invalid value for %.*s; %d\n",
				name->len, name->s, m);
	*val=(void*)(long)f;
	return ret;
}


/**
 * This function cancels a previously created local invite
 * transaction. The cancel parameter should NOT have any via (CANCEL is
 * hop by hop). returns 0 if error return >0 if OK (returns the LABEL of
 * the cancel).*/
unsigned int t_uac_cancel( str *headers, str *body,
		unsigned int cancelled_hashIdx, unsigned int cancelled_label,
		transaction_cb cb, void* cbp)
{
	struct cell *t_invite,*cancel_cell;
	struct retr_buf *cancel,*invite;
	unsigned int len,ret;
	char *buf;

	ret=0;
	if(t_lookup_ident(&t_invite,cancelled_hashIdx,cancelled_label)<0){
		LM_ERR("failed to t_lookup_ident hash_idx=%d,"
				"label=%d\n", cancelled_hashIdx,cancelled_label);
		return 0;
	}
	/* <sanity_checks> */
	if(! is_local(t_invite))
	{
		LM_ERR("tried to cancel a non-local transaction\n");
		goto error3;
	}
	if(t_invite->uac[0].last_received < 100)
	{
		LM_WARN("trying to cancel a transaction not in "
					"Proceeding state !\n");
		goto error3;
	}
	if(t_invite->uac[0].last_received > 200)
	{
		LM_WARN("trying to cancel a completed transaction !\n");
		goto error3;
	}
	/* </sanity_checks*/
	/* <build_cell> */
	if(!(cancel_cell = build_cell(0))){
		ret=0;
		LM_ERR("no more shm memory!\n");
		goto error3;
	}
	reset_avps();
	if(cb && insert_tmcb(&(cancel_cell->tmcb_hl),
			TMCB_RESPONSE_IN|TMCB_LOCAL_COMPLETED,cb,cbp,0)!=1){
		ret=0;
		LM_ERR("short of tmcb shmem !\n");
		goto error2;
	}
	/* </build_cell> */

	/* <insert_into_hashtable> */
	cancel_cell->flags |= T_IS_LOCAL_FLAG;
	cancel_cell->hash_index=t_invite->hash_index;

	LOCK_HASH(cancel_cell->hash_index);
	insert_into_hash_table_unsafe(cancel_cell,cancel_cell->hash_index);
	ret=cancel_cell->label;
	cancel_cell->label=t_invite->label;
	UNLOCK_HASH(cancel_cell->hash_index);
	/* </insert_into_hashtable> */

	/* <prepare_cancel> */

	cancel=&cancel_cell->uac[0].request;
	invite=&t_invite->uac[0].request;

	cancel->dst.to              = invite->dst.to;
	cancel->dst.send_sock       = invite->dst.send_sock;
	cancel->dst.proto           = invite->dst.proto;
	//cancel->dst.proto_reserved1 = invite->dst.proto_reserved1;

	if(!(buf = build_uac_cancel(headers,body,t_invite,0,&len,
					&(cancel->dst)))){
		ret=0;
		LM_ERR("attempt to build a CANCEL failed\n");
		goto error1;
	}
	cancel->buffer=buf;
	cancel->buffer_len=len;
	cancel_cell->method.s = buf;
	cancel_cell->method.len = 6 /*c-a-n-c-e-l*/;
	/* </prepare_cancel> */

	/* <strart_sending> */
	cancel_cell->nr_of_outgoings++;
	if (SEND_BUFFER(cancel)==-1) {
		ret=0;
		LM_ERR("send failed\n");
		goto error1;
	}
	start_retr(cancel);
	/* </start_sending> */

	return ret;

error1:
	LOCK_HASH(cancel_cell->hash_index);
	remove_from_hash_table_unsafe(cancel_cell);
	UNLOCK_HASH(cancel_cell->hash_index);
error2:
	free_cell(cancel_cell);
error3:
	return ret;
}


