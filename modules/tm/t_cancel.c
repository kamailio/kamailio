/*
 * $Id$
 *
 */


#include "t_funcs.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "t_reply.h"
#include "t_cancel.h"
#include "t_msgbuilder.h"


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
	int len;
	struct retr_buf *crb, *irb;

	crb=&t->uac[branch].local_cancel;
	irb=&t->uac[branch].request;

#	ifdef EXTRA_DEBUG
	if (crb->buffer!=0 && crb->buffer!=BUSY_BUFFER) {
		LOG(L_CRIT, "ERROR: attempt to rewrite cancel buffer\n");
		abort();
	}
#	endif

	cancel=build_cancel(t, branch, &len);
	if (!cancel) {
		LOG(L_ERR, "ERROR: attempt to build a CANCEL failed\n");
		return;
	}
	/* install cancel now */
	crb->buffer=cancel;
	crb->buffer_len=len;
	crb->to=irb->to;
	crb->send_sock=irb->send_sock;
	crb->branch=branch;
#ifdef _OBSOLETED
	crb->fr_timer.tg=TG_FR;
	crb->retr_timer.tg=TG_RT;
	crb->my_T=t;
#endif
	crb->retr_timer.payload=crb->fr_timer.payload=crb;
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

