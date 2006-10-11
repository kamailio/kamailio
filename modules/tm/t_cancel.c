/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * ----------
 * 2003-04-14  checking if a reply sent before cancel is initiated
 *             moved here (jiri)
 * 2004-02-11  FIFO/CANCEL + alignments (hash=f(callid,cseq)) (uli+jiri)
 * 2004-02-13  timer_link.payload removed (bogdan)
 * 2006-10-10  cancel_uacs  & cancel_branch take more options now (andrei)
 */

#include <stdio.h> /* for FILE* in fifo_uac_cancel */

#include "defs.h"


#include "t_funcs.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "t_reply.h"
#include "t_cancel.h"
#include "t_msgbuilder.h"
#include "t_lookup.h" /* for t_lookup_callid in fifo_uac_cancel */


/* determine which branches should be canceled; do it
   only from within REPLY_LOCK, otherwise collisions
   could occur (e.g., two 200 for two branches processed
   by two processes might concurrently try to generate
   a CANCEL for the third branch, resulting in race conditions
   during writing to cancel buffer
 WARNING: - has side effects, see should_cancel_branch() */
void which_cancel( struct cell *t, branch_bm_t *cancel_bm )
{
	int i;
	
	*cancel_bm=0;
	for( i=0 ; i<t->nr_of_outgoings ; i++ ) {
		if (should_cancel_branch(t, i)) 
			*cancel_bm |= 1<<i ;

	}
}




/* cancel branches scheduled for deletion
 * params: t          - transaction
 *          cancel_bm - bitmap with the branches that are supposed to be 
 *                       canceled 
 *          flags     - how_to_cancel flags, see cancel_branch()
 * returns: bitmap with the still active branches (on fr timer)
 * WARNING: always fill cancel_bm using which_cancel(), supplying values
 *          in any other way is a bug*/
int cancel_uacs( struct cell *t, branch_bm_t cancel_bm, int flags)
{
	int i;
	int ret;
	int r;

	ret=0;
	/* cancel pending client transactions, if any */
	for( i=0 ; i<t->nr_of_outgoings ; i++ ) 
		if (cancel_bm & (1<<i)){
			r=cancel_branch(t, i, flags);
			ret|=(r!=0)<<i;
		}
	return ret;
}



/* 
 * params:  t - transaction
 *          branch - branch number to be canceled
 *          flags - howto cancel: 
 *                   F_CANCEL_B_KILL - will completely stop the 
 *                     branch (stops the timers), use with care
 *                   F_CANCEL_B_FAKE_REPLY - will send a fake 487
 *                      to all branches that haven't received any response
 *                      (>=100). It assumes the REPLY_LOCK is not held
 *                      (if it is => deadlock)
 *                  default: stop only the retransmissions for the branch
 *                      and leave it to timeout if it doesn't receive any
 *                      response to the CANCEL
 * returns: 0 - branch inactive after running cancel_branch() 
 *          1 - branch still active  (fr_timer)
 *         -1 - error
 * WARNING:
 *          - F_CANCEL_KILL_B should be used only if the transaction is killed
 *            explicitely afterwards (since it might kill all the timers
 *            the transaction won't be able to "kill" itself => if not
 *            explicitely "put_on_wait" it migh leave forever)
 *          - F_CANCEL_B_FAKE_REPLY must be used only if the REPLY_LOCK is not
 *            held
 */
int cancel_branch( struct cell *t, int branch, int flags )
{
	char *cancel;
	unsigned int len;
	struct retr_buf *crb, *irb;
	int ret;
	branch_bm_t tmp_bm;

	crb=&t->uac[branch].local_cancel;
	irb=&t->uac[branch].request;
	ret=1;

#	ifdef EXTRA_DEBUG
	if (crb->buffer!=0 && crb->buffer!=BUSY_BUFFER) {
		LOG(L_CRIT, "ERROR: attempt to rewrite cancel buffer\n");
		abort();
	}
#	endif

	if (flags & F_CANCEL_B_KILL){
		stop_rb_timers( irb );
		ret=0;
		if (t->uac[branch].last_received < 100) {
			DBG("DEBUG: cancel_branch: no response ever received: "
			    "giving up on cancel\n");
			if (flags & F_CANCEL_B_FAKE_REPLY){
				LOCK_REPLIES(t);
				if (relay_reply(t, FAKED_REPLY, branch, 487, &tmp_bm) == 
										RPS_ERROR){
					return -1;
				}
			}
			/* do nothing, hope that the caller will clean up */
			return ret;
		}
	}else{
		stop_rb_retr(irb); /* stop retransmissions */
		if (t->uac[branch].last_received < 100) {
			/* no response received => don't send a cancel on this branch,
			 *  just drop it */
			if (flags & F_CANCEL_B_FAKE_REPLY){
				stop_rb_timers( irb ); /* stop even the fr timer */
				LOCK_REPLIES(t);
				if (relay_reply(t, FAKED_REPLY, branch, 487, &tmp_bm) == 
										RPS_ERROR){
					return -1;
				}
				return 0; /* should be inactive after the 487 */
			}
			/* do nothing, just wait for the final timeout */
			return 1;
		}
	}
	
	cancel = build_local(t, branch, &len, CANCEL, CANCEL_LEN, &t->to);
	if (!cancel) {
		LOG(L_ERR, "ERROR: attempt to build a CANCEL failed\n");
		return -1;
	}
	/* install cancel now */
	crb->buffer = cancel;
	crb->buffer_len = len;
	crb->dst = irb->dst;
	crb->branch = branch;
	/* label it as cancel so that FR timer can better know how to
	   deal with it */
	crb->activ_type = TYPE_LOCAL_CANCEL;

	DBG("DEBUG: cancel_branch: sending cancel...\n");
	SEND_BUFFER( crb );
	/*sets and starts the FINAL RESPONSE timer */
	if (start_retr( crb )!=0)
		LOG(L_CRIT, "BUG: cancel_branch: failed to start retransmission"
					" for %p\n", crb);
	return ret;
}


const char* rpc_cancel_doc[2] = {
	"Cancel a pending transaction",
	0
};


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
	branch_bm_t cancel_bm;
	int i,j;

	str cseq_s;   /* cseq */
	str callid_s; /* callid */

	cseq_s.s=cseq;
	callid_s.s=callid;
	cancel_bm=0;

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
	LOCK_REPLIES(trans);
		which_cancel(trans, &cancel_bm);
	UNLOCK_REPLIES(trans);
	 /* tell tm to cancel the call */
	DBG("Now calling cancel_uacs\n");
	i=cancel_uacs(trans, cancel_bm, 0); /* don't fake 487s, 
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
