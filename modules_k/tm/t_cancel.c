/*
 * $Id$
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * ----------
 * 2003-04-14  checking if a reply sent before cancel is initiated
 *             moved here (jiri)
 * 2004-02-11  FIFO/CANCEL + alignments (hash=f(callid,cseq)) (uli+jiri)
 * 2004-02-13  timer_link.payload removed (bogdan)
 */

/*! \file
 * \brief TM :: CANCEL processing
 *
 * \ingroup tm
 * - Module: \ref tm
 */

#include "t_funcs.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "t_reply.h"
#include "t_fwd.h"
#include "t_cancel.h"
#include "t_msgbuilder.h"
#include "t_lookup.h" /* for t_lookup_callid in fifo_uac_cancel */


/*! \brief
   Determine which branches should be canceled.

   Do it only from within REPLY_LOCK, otherwise collisions
   could occur (e.g., two 200 for two branches processed
   by two processes might concurrently try to generate
   a CANCEL for the third branch, resulting in race conditions
   during writing to cancel buffer
*/
void which_cancel( struct cell *t, branch_bm_t *cancel_bm )
{
	int i;

	for( i=t->first_branch ; i<t->nr_of_outgoings ; i++ ) {
		if (should_cancel_branch(t, i)) 
			*cancel_bm |= 1<<i ;

	}
}


/*! \brief cancel branches scheduled for deletion */
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
	if (crb->buffer.s!=0 && crb->buffer.s!=BUSY_BUFFER) {
		LM_CRIT("attempt to rewrite cancel buffer failed\n");
		abort();
	}
#	endif

	cancel=build_cancel(t, branch, &len);
	if (!cancel) {
		LM_ERR("attempt to build a CANCEL failed\n");
		return;
	}
	/* install cancel now */
	crb->buffer.s=cancel;
	crb->buffer.len=len;
	crb->dst=irb->dst;
	crb->branch=branch;
	/* label it as cancel so that FR timer can better now how 
	 * to deal with it */
	crb->activ_type=TYPE_LOCAL_CANCEL;

	if ( has_tran_tmcbs( t, TMCB_REQUEST_BUILT) ) {
		set_extra_tmcb_params( &crb->buffer, &crb->dst);
		run_trans_callbacks( TMCB_REQUEST_BUILT, t, t->uas.request,0,
			-t->uas.request->REQ_METHOD);
	}

	LM_DBG("sending cancel...\n");
	SEND_BUFFER( crb );

	/*sets and starts the FINAL RESPONSE timer */
	start_retr( crb );
}


char *build_cancel(struct cell *Trans,unsigned int branch,
	unsigned int *len )
{
	return build_local( Trans, branch, len,
		CANCEL, CANCEL_LEN, &Trans->to );
}


/*! \brief
 * This function cancels a previously created local invite transaction.
 *
 * The cancel parameter should NOT have any via (CANCEL is hop-by-hop).
 *
 * \return 0 if error
 * \return >0 if OK (returns the LABEL of the cancel).
 */
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
	if(! is_local(t_invite)){
		LM_ERR("tried to cancel a non-local transaction\n");
		goto error3;
	}
	if(t_invite->uac[0].last_received < 100){
		LM_WARN("trying to cancel a transaction not in "
			"Proceeding state !\n");
		goto error3;
	}
	if(t_invite->uac[0].last_received > 200){
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
	cancel->dst.proto_reserved1 = invite->dst.proto_reserved1;

	if(!(buf = build_uac_cancel(headers,body,t_invite,0,&len))){
		ret=0;
		LM_ERR("attempt to build a CANCEL failed\n");
		goto error1;
	}
	cancel->buffer.s=buf;
	cancel->buffer.len=len;
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

