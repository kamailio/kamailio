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
 *  2009-06-01  Pre- and post-script callbacks of failure route are executed (Miklos)
 *
 */

#include "../../action.h"
#include "../../script_cb.h"
#include "../../dset.h"

#include "config.h"
#include "sip_msg.h"
#include "t_reply.h"
#include "h_table.h"
#include "t_lookup.h"
#include "t_fwd.h"
#include "t_funcs.h"
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

	if (t->flags & T_CANCELED) {
		/* The transaction has already been canceled */
		LOG(L_DBG, "DEBUG: t_suspend: " \
			"trying to suspend an already canceled transaction\n");
		ser_error = E_CANCELED;
		return 1;
	}

	/* send a 100 Trying reply, because the INVITE processing
	will probably take a long time */
	if (msg->REQ_METHOD==METHOD_INVITE && (t->flags&T_AUTO_INV_100)
		&& (t->uas.status < 100)
	) {
		if (!t_reply( t, msg , 100 ,
			cfg_get(tm, tm_cfg, tm_auto_inv_100_r)))
				DBG("SER: ERROR: t_suspend (100)\n");
	}

	if ((t->nr_of_outgoings==0) && /* if there had already been
				an UAC created, then the lumps were
				saved as well */
		save_msg_lumps(t->uas.request, msg)
	) {
		LOG(L_ERR, "ERROR: t_suspend: " \
			"failed to save the message lumps\n");
		return -1;
	}
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

	/* backup some extra info that can be used in continuation logic */
	t->async_backup.backup_route = get_route_type();
	t->async_backup.backup_branch = get_t_branch();
	t->async_backup.ruri_new = ruri_get_forking_state();


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
	struct ua_client *uac =NULL;
	int	ret;

	if (t_lookup_ident(&t, hash_index, label) < 0) {
		LOG(L_ERR, "ERROR: t_continue: transaction not found\n");
		return -1;
	}

	if (t->flags & T_CANCELED) {
		/* The transaction has already been canceled,
		 * needless to continue */
		UNREF(t); /* t_unref would kill the transaction */
		/* reset T as we have no working T anymore */
		set_t(T_UNDEFINED, T_BR_UNDEFINED);
		return 1;
	}

	/* The transaction has to be locked to protect it
	 * form calling t_continue() multiple times simultaneously */
	LOCK_ASYNC_CONTINUE(t);
	t->flags |= T_ASYNC_CONTINUE;   /* we can now know anywhere in kamailio 
					 * that we are executing post a suspend */
	
	/* which route block type were we in when we were suspended */
	int cb_type = REQUEST_CB_TYPE;
        switch (t->async_backup.backup_route) {
            case REQUEST_ROUTE:
                cb_type = REQUEST_CB_TYPE;
                break;
            case FAILURE_ROUTE:
                cb_type = FAILURE_CB_TYPE;
                break;
            case TM_ONREPLY_ROUTE:
                 cb_type = ONREPLY_CB_TYPE;
                break;
            case BRANCH_ROUTE:
                cb_type = BRANCH_CB_TYPE;
                break;
        }
	
	branch = t->async_backup.blind_uac;	/* get the branch of the blind UAC setup 
						 * during suspend */
	if (branch >= 0) {
		stop_rb_timers(&t->uac[branch].request);

		if (t->uac[branch].last_received != 0) {
			/* Either t_continue() has already been
			 * called or the branch has already timed out.
			 * Needless to continue. */
			UNLOCK_ASYNC_CONTINUE(t);
			UNREF(t); /* t_unref would kill the transaction */
			return 1;
		}

		/*we really don't need this next line anymore otherwise we will 
		  never be able to forward replies after a (t_relay) on this branch.
		  We want to try and treat this branch as 'normal' (as if it were a normal req, not async)' */
		//t->uac[branch].last_received=500;
		uac = &t->uac[branch];
	}
	/* else
		Not a huge problem, fr timer will fire, but CANCEL
		will not be sent. last_received will be set to 408. */

	reset_kr();

	/* fake the request and the environment, like in failure_route */
	if (!fake_req(&faked_req, t->uas.request, 0 /* extra flags */, uac)) {
		LOG(L_ERR, "ERROR: t_continue: fake_req failed\n");
		ret = -1;
		goto kill_trans;
	}
	faked_env( t, &faked_req, 1);

	/* execute the pre/post -script callbacks based on original route block */
	if (exec_pre_script_cb(&faked_req, cb_type)>0) {
		if (run_top_route(route, &faked_req, 0)<0)
			LOG(L_ERR, "ERROR: t_continue: Error in run_top_route\n");
		exec_post_script_cb(&faked_req, cb_type);
	}

	/* TODO: save_msg_lumps should clone the lumps to shm mem */

	/* restore original environment and free the fake msg */
	faked_env( t, 0, 1);
	free_faked_req(&faked_req, t);

	/* update the flags */
	t->uas.request->flags = faked_req.flags;

	if (t->uas.status < 200) {
		/* No final reply has been sent yet.
		 * Check whether or not there is any pending branch.
		 */
		for (	branch = 0;
			branch < t->nr_of_outgoings;
			branch++
		) {
			if (t->uac[branch].last_received < 200)
				break;
		}

		if (branch == t->nr_of_outgoings) {
			/* There is not any open branch so there is
			 * no chance that a final response will be received. */
			ret = 0;
			goto kill_trans;
		}
	}

	UNLOCK_ASYNC_CONTINUE(t);

	/* unref the transaction */
	t_unref(t->uas.request);

	return 0;

kill_trans:
	/* The script has hopefully set the error code. If not,
	 * let us reply with a default error. */
	if ((kill_transaction_unsafe(t,
		tm_error ? tm_error : E_UNSPEC)) <=0
	) {
		LOG(L_ERR, "ERROR: t_continue: "
			"reply generation failed\n");
		/* The transaction must be explicitely released,
		 * no more timer is running */
		UNLOCK_ASYNC_CONTINUE(t);
		t_release_transaction(t);
	} else {
		UNLOCK_ASYNC_CONTINUE(t);
	}

	t_unref(t->uas.request);
	return ret;
}

/* Revoke the suspension of the SIP request, i.e.
 * cancel the fr timer of the blind uac.
 * This function can be called when something fails
 * after t_suspend() has already been executed in the same
 * process, and it turns out that the transaction should
 * not have been suspended.
 * 
 * Return value:
 * 	0  - success
 * 	<0 - failure
 */
int t_cancel_suspend(unsigned int hash_index, unsigned int label)
{
	struct cell	*t;
	int	branch;
	
	t = get_t();
	if (!t || t == T_UNDEFINED) {
		LOG(L_ERR, "ERROR: t_revoke_suspend: " \
			"no active transaction\n");
		return -1;
	}
	/* Only to double-check the IDs */
	if ((t->hash_index != hash_index)
		|| (t->label != label)
	) {
		LOG(L_ERR, "ERROR: t_revoke_suspend: " \
			"transaction id mismatch\n");
		return -1;
	}
	/* The transaction does not need to be locked because this
	 * function is either executed from the original route block
	 * or from failure route which already locks */

	reset_kr(); /* the blind UAC of t_suspend has set kr */

	/* Try to find the blind UAC, and cancel its fr timer.
	 * We assume that the last blind uac called this function. */
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
	} else {
		/* Not a huge problem, fr timer will fire, but CANCEL
		will not be sent. last_received will be set to 408. */
		return -1;
	}

	return 0;
}
