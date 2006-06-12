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
*/


void which_cancel( struct cell *t, branch_bm_t *cancel_bm )
{
	int i;

	for( i=0 ; i<t->nr_of_outgoings ; i++ ) {
		if (should_cancel_branch(t, i)) 
			*cancel_bm |= 1<<i ;

	}
}


/* cancel branches scheduled for deletion */
void cancel_uacs( struct cell *t, branch_bm_t cancel_bm )
{
	int i;

	/* cancel pending client transactions, if any */
	for( i=0 ; i<t->nr_of_outgoings ; i++ ) 
		if (cancel_bm & (1<<i))
           	cancel_branch(t, i);
}

void cancel_branch( struct cell *t, int branch )
{
	char *cancel;
	unsigned int len;
	struct retr_buf *crb, *irb;

	crb=&t->uac[branch].local_cancel;
	irb=&t->uac[branch].request;

#	ifdef EXTRA_DEBUG
	if (crb->buffer!=0 && crb->buffer!=BUSY_BUFFER) {
		LOG(L_CRIT, "ERROR: attempt to rewrite cancel buffer\n");
		abort();
	}
#	endif

	stop_rb_timers( irb );

	if (t->uac[branch].last_received < 100) {
		DBG("DEBUG: cancel_branch: no response ever received: "
		    "giving up on cancel\n");
		return;
	}
	
	cancel = build_local(t, branch, &len, CANCEL, CANCEL_LEN, &t->to);
	if (!cancel) {
		LOG(L_ERR, "ERROR: attempt to build a CANCEL failed\n");
		return;
	}
	/* install cancel now */
	crb->buffer = cancel;
	crb->buffer_len = len;
	crb->dst = irb->dst;
	crb->branch = branch;
	/* label it as cancel so that FR timer can better now how to
	   deal with it */
	crb->activ_type = TYPE_LOCAL_CANCEL;

	DBG("DEBUG: cancel_branch: sending cancel...\n");
	SEND_BUFFER( crb );
	/*sets and starts the FINAL RESPONSE timer */
	if (start_retr( crb )!=0)
		LOG(L_CRIT, "BUG: cancel_branch: failed to start retransmission"
					" for %p\n", crb);
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

	str cseq_s;   /* cseq */
	str callid_s; /* callid */

	cseq_s.s=cseq;
	callid_s.s=callid;

	if (rpc->scan(rpc, "SS", &callid_s, &cseq_s) < 2) {
		rpc->fault(c, 400, "Callid and CSeq expected as parameters");
		return;
	}

	if( t_lookup_callid(&trans, callid_s, cseq_s) < 0 ) {
		DBG("Lookup failed\n");
		rpc->fault(c, 400, "Transaction not found");
		return;
	}
	     /* tell tm to cancel the call */
	DBG("Now calling cancel_uacs\n");
	(*cancel_uacs)(trans, ~0);
	
	     /* t_lookup_callid REF`d the transaction for us, we must UNREF here! */
	UNREF(trans);
}
