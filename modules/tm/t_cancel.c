/*
 * $Id$
 *
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
#include "../../fifo_server.h" /* for read_line() and fifo_reply() */
#include "../../unixsock_server.h"


/* determine which branches should be cancelled; do it
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

	if (t->uac[branch].last_received<100) {
		DBG("DEBUG: cancel_branch: no response ever received: "
		    "giving up on cancel\n");
		return;
	}

	cancel=build_cancel(t, branch, &len);
	if (!cancel) {
		LOG(L_ERR, "ERROR: attempt to build a CANCEL failed\n");
		return;
	}
	/* install cancel now */
	crb->buffer=cancel;
	crb->buffer_len=len;
	crb->dst=irb->dst;
	crb->branch=branch;
	/* TO_REMOVE
	crb->retr_timer.payload=crb->fr_timer.payload=crb;
	*/
	/* label it as cancel so that FR timer can better now how to
	   deal with it */
	crb->activ_type=TYPE_LOCAL_CANCEL;

    DBG("DEBUG: cancel_branch: sending cancel...\n");
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

/* fifo command to cancel a pending call (Uli)
  Syntax:

  ":uac_cancel:[response file]\n
  callid\n
 cseq\n
  \n"
 */

int fifo_uac_cancel( FILE* stream, char *response_file )
{
	struct cell *trans;
	static char cseq[128], callid[128];

	str cseq_s;   /* cseq */
	str callid_s; /* callid */

	cseq_s.s=cseq;
	callid_s.s=callid;

	DBG("DEBUG: fifo_uac_cancel: ############### begin ##############\n");

	/* first param callid read */
	if (!read_line(callid_s.s, 128, stream, &callid_s.len)||callid_s.len==0) {
		LOG(L_ERR, "ERROR: fifo_uac_cancel: callid expected\n");
		fifo_reply(response_file, "400 fifo_uac_cancel: callid expected");
		return -1;
	}
	callid_s.s[callid_s.len]='\0';
	DBG("DEBUG: fifo_uac_cancel: callid=\"%.*s\"\n",callid_s.len, callid_s.s);

	/* second param cseq read */
	if (!read_line(cseq_s.s, 128, stream, &cseq_s.len)||cseq_s.len==0) {
		LOG(L_ERR, "ERROR: fifo_uac_cancel: cseq expected\n");
		fifo_reply(response_file, "400 fifo_uac_cancel: cseq expected");
		return -1;
	}
	cseq_s.s[cseq_s.len]='\0';
	DBG("DEBUG: fifo_uac_cancel: cseq=\"%.*s\"\n",cseq_s.len, cseq_s.s);

	if( t_lookup_callid(&trans, callid_s, cseq_s) < 0 ) {
		LOG(L_ERR,"ERROR: fifo_uac_cancel: lookup failed\n");
		fifo_reply(response_file, "481 fifo_uac_cancel: no such transaction");
		return -1;
	}
	/* tell tm to cancel the call */
	DBG("DEBUG: fifo_uac_cancel: now calling cancel_uacs\n");
	(*cancel_uacs)(trans,~0);

	/* t_lookup_callid REF`d the transaction for us, we must UNREF here! */
	UNREF(trans);

	fifo_reply(response_file, "200 fifo_uac_cancel succeeded\n");
	DBG("DEBUG: fifo_uac_cancel: ################ end ##############\n");
	return 1;
}


int unixsock_uac_cancel(str* msg)
{
	struct cell *trans;
	str cseq, callid;

	     /* first param callid read */
	if (unixsock_read_line(&callid, msg) != 0) {
		unixsock_reply_asciiz("400 Call-ID Expected\n");
		unixsock_reply_send();
		return -1;
	}

	     /* second param cseq read */
	if (unixsock_read_line(&callid, msg) != 0) {
		unixsock_reply_asciiz("400 CSeq Expected\n");
		unixsock_reply_send();
		return -1;
	}

	if (t_lookup_callid(&trans, callid, cseq) < 0) {
		LOG(L_ERR, "unixsock_uac_cancel: Lookup failed\n");
		unixsock_reply_asciiz("481 uac_cancel: No such transaction\n");
		unixsock_reply_send();
		return 1;
	}

	     /* tell tm to cancel the call */
	(*cancel_uacs)(trans, ~0);

	     /* t_lookup_callid REF`d the transaction for us, we must UNREF here! */
	UNREF(trans);

	unixsock_reply_asciiz("200 uac_cancel succeeded\n");
	unixsock_reply_send();
	return 0;
}
