/*
 * $Id$
 *
 * Copyright (C) 2008 iptelorg GmbH
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
 *  2008-11-10	Initial version (Miklos)
 *
 */

#include "sip_msg.h"
#include "t_reply.h"
#include "h_table.h"
#include "t_lookup.h"
#include "t_fwd.h"
#include "timer.h"
#include "t_suspend.h"

/* Suspends the transaction for later use.
 * Save the returned hash_index and label to get
 * back to the SIP request processing, see the readme.
 *
 * Return value:
 * 	0  - success
 * 	<0 - failure
 */
int t_suspend(struct sip_msg *msg,
		unsigned int *hash_index, unsigned int *label)
{
	struct cell	*t;

	t = get_t();
	if (!t || t == T_UNDEFINED) {
		LOG(L_ERR, "ERROR: t_suspend: " \
			"transaction has not been created yet\n");
		return -1;
	}

	/* send a 100 Trying reply, because the INVITE processing
	will probably take a long time */
	if (msg->REQ_METHOD==METHOD_INVITE && (t->flags&T_AUTO_INV_100)) {
		if (!t_reply( t, msg , 100 ,
			"trying -- your call is important to us"))
				DBG("SER: ERROR: t_suspend (100)\n");
	}

#ifdef POSTPONE_MSG_CLONING
	if ((t->nr_of_outgoings==0) && /* if there had already been
				an UAC created, then the lumps were
				saved as well */
		save_msg_lumps(t->uas.request, msg)
	) {
		LOG(L_ERR, "ERROR: t_suspend: " \
			"failed to save the message lumps\n");
		return -1;
	}
#endif
	/* save the message flags */
	t->uas.request->flags = msg->flags;

	*hash_index = t->hash_index;
	*label = t->label;

	/* add a bling UAC to let the fr timer running */
	if (add_blind_uac() < 0) {
		LOG(L_ERR, "ERROR: t_suspend: " \
			"failed to add the blind UAC\n");
		return -1;
	}

	return 0;
}

/* Continues the SIP request processing previously saved by
 * t_suspend(). The script does not continue from the same
 * point, but a separate route block is executed instead.
 *
 * Return value:
 * 	0  - success
 * 	<0 - failure
 */
int t_continue(unsigned int hash_index, unsigned int label,
		struct action *route)
{
	struct cell	*t;
	struct sip_msg	faked_req;
	int	branch;

	if (t_lookup_ident(&t, hash_index, label) < 0) {
		LOG(L_ERR, "ERROR: t_continue: transaction not found\n");
		return -1;
	}

	/* The transaction has to be locked to protect it
	 * form calling t_continue() multiple times simultaneously */
	LOCK_REPLIES(t);

	/* Try to find the blind UAC, and cancel its fr timer.
	 * We assume that the last blind uac called t_continue(). */
	for (	branch = t->nr_of_outgoings-1;
		branch >= 0 && t->uac[branch].request.buffer;
		branch--);

	if (branch >= 0) {
		stop_rb_timers(&t->uac[branch].request);
		/* Set last_received to something >= 200,
		 * the actual value does not matter, the branch
		 * will never be picked up for response forwarding.
		 * If last_received is lower than 200,
		 * then the branch may tried to be cancelled later,
		 * for example when t_reply() is called from
		 * a failure rute => deadlock, because both
		 * of them need the reply lock to be held. */
		t->uac[branch].last_received=500;
	}
	/* else
		Not a huge problem, fr timer will fire, but CANCEL
		will not be sent. last_received will be set to 408. */

	/* fake the request and the environment, like in failure_route */
	if (!fake_req(&faked_req, t->uas.request, 0 /* extra flags */)) {
		LOG(L_ERR, "ERROR: t_continue: fake_req failed\n");
		UNLOCK_REPLIES(t);
		return -1;
	}
	faked_env( t, &faked_req);

	if (run_top_route(route, &faked_req)<0)
		LOG(L_ERR, "ERROR: t_continue: Error in run_top_route\n");

	/* TODO: save_msg_lumps should clone the lumps to shm mem */

	/* restore original environment and free the fake msg */
	faked_env( t, 0);
	free_faked_req(&faked_req, t);

	/* update the flags */
	t->uas.request->flags = faked_req.flags;

	UNLOCK_REPLIES(t);

	/* release the transaction */
	t_unref(t->uas.request);

	return 0;
}
