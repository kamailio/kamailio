/*
 * Copyright (C) 2008 iptelorg GmbH
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
#include "t_cancel.h"

#include "t_suspend.h"

#include "../../data_lump.h"
#include "../../data_lump_rpl.h"


#ifdef ENABLE_ASYNC_MUTEX
#define LOCK_ASYNC_CONTINUE(_t) lock(&(_t)->async_mutex )
#define UNLOCK_ASYNC_CONTINUE(_t) unlock(&(_t)->async_mutex )
#else
#define LOCK_ASYNC_CONTINUE(_t) LOCK_REPLIES(_t)
#define UNLOCK_ASYNC_CONTINUE(_t) UNLOCK_REPLIES(_t)
#endif

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
	int branch;
	int sip_msg_len;

	t = get_t();
	if (!t || t == T_UNDEFINED) {
		LM_ERR("transaction has not been created yet\n");
		return -1;
	}

	if (t->flags & T_CANCELED) {
		/* The transaction has already been canceled */
		LM_DBG("trying to suspend an already canceled transaction\n");
		ser_error = E_CANCELED;
		return 1;
	}

	if (msg->first_line.type != SIP_REPLY) {
		/* send a 100 Trying reply, because the INVITE processing
		will probably take a long time */
		if (msg->REQ_METHOD==METHOD_INVITE && (t->flags&T_AUTO_INV_100)
			&& (t->uas.status < 100)
		) {
			if (!t_reply( t, msg , 100 ,
				cfg_get(tm, tm_cfg, tm_auto_inv_100_r)))
				LM_DBG("suspending request processing - sending 100 reply\n");
		}

		if ((t->nr_of_outgoings==0) && /* if there had already been
			an UAC created, then the lumps were
			saved as well */
			save_msg_lumps(t->uas.request, msg)
		) {
			LM_ERR("failed to save the message lumps\n");
			return -1;
		}
		/* save the message flags */
		t->uas.request->flags = msg->flags;

		/* add a blind UAC to let the fr timer running */
		if (add_blind_uac() < 0) {
			LM_ERR("failed to add the blind UAC\n");
			return -1;
		}
		/* propagate failure route to new branch
		 * - failure route to be executed if the branch is not continued
		 *   before timeout */
		t->uac[t->async_backup.blind_uac].on_failure = t->on_failure;
	} else {
		LM_DBG("this is a suspend on reply - setting msg flag to SUSPEND\n");
		msg->msg_flags |= FL_RPL_SUSPENDED;
                t->flags |= T_ASYNC_SUSPENDED;
		/* this is a reply suspend find which branch */

		if (t_check( msg  , &branch )==-1){
			LOG(L_ERR, "ERROR: t_suspend_reply: " \
				"failed find UAC branch\n");
			return -1; 
		}
		LM_DBG("found a a match with branch id [%d] - "
				"cloning reply message to t->uac[branch].reply\n", branch);

		sip_msg_len = 0;
		t->uac[branch].reply = sip_msg_cloner( msg, &sip_msg_len );

		if (! t->uac[branch].reply ) {
			LOG(L_ERR, "can't alloc' clone memory\n");
			return -1;
		}
		t->uac[branch].end_reply = ((char*)t->uac[branch].reply) + sip_msg_len;

		LM_DBG("saving transaction data\n");
		t->uac[branch].reply->flags = msg->flags;
	}

	*hash_index = t->hash_index;
	*label = t->label;



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
	struct cancel_info cancel_data;
	int	branch;
	struct ua_client *uac =NULL;
	int	ret;
	int cb_type;
	int msg_status;
	int last_uac_status;
	int reply_status;
	int do_put_on_wait;
	struct hdr_field *hdr, *prev = 0, *tmp = 0;

	if (t_lookup_ident(&t, hash_index, label) < 0) {
		LM_ERR("transaction not found\n");
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
	cb_type =  FAILURE_CB_TYPE;;
	switch (t->async_backup.backup_route) {
		case REQUEST_ROUTE:
			cb_type = FAILURE_CB_TYPE;
			break;
		case FAILURE_ROUTE:
			cb_type = FAILURE_CB_TYPE;
			break;
		case TM_ONREPLY_ROUTE:
			cb_type = ONREPLY_CB_TYPE;
			break;
		case BRANCH_ROUTE:
			cb_type = FAILURE_CB_TYPE;
			break;
	}

	if(t->async_backup.backup_route != TM_ONREPLY_ROUTE){
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

			/* Set last_received to something >= 200,
			 * the actual value does not matter, the branch
			 * will never be picked up for response forwarding.
			 * If last_received is lower than 200,
			 * then the branch may tried to be cancelled later,
			 * for example when t_reply() is called from
			 * a failure route => deadlock, because both
			 * of them need the reply lock to be held. */
			t->uac[branch].last_received=500;
			uac = &t->uac[branch];
		}
		/* else
			Not a huge problem, fr timer will fire, but CANCEL
			will not be sent. last_received will be set to 408. */

		/* We should not reset kr here to 0 as it's quite possible before continuing the dev. has correctly set the
		 * kr by, for example, sending a transactional reply in code - resetting here will cause a dirty log message
		 * "WARNING: script writer didn't release transaction" to appear in log files. TODO: maybe we need to add 
		 * a special kr for async?
		 * reset_kr();
		 */

		/* fake the request and the environment, like in failure_route */
		if (!fake_req(&faked_req, t->uas.request, 0 /* extra flags */, uac)) {
			LM_ERR("building fake_req failed\n");
			ret = -1;
			goto kill_trans;
		}
		faked_env( t, &faked_req, 1);

		/* execute the pre/post -script callbacks based on original route block */
		if (exec_pre_script_cb(&faked_req, cb_type)>0) {
			if (run_top_route(route, &faked_req, 0)<0)
				LM_ERR("failure inside run_top_route\n");
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

	} else {
		branch = t->async_backup.backup_branch;

		init_cancel_info(&cancel_data);

		LM_DBG("continuing from a suspended reply"
				" - resetting the suspend branch flag\n");

		t->uac[branch].reply->msg_flags &= ~FL_RPL_SUSPENDED;
		if (t->uas.request) t->uas.request->msg_flags&= ~FL_RPL_SUSPENDED;

		faked_env( t, t->uac[branch].reply, 1);

		if (exec_pre_script_cb(t->uac[branch].reply, cb_type)>0) {
			if (run_top_route(route, t->uac[branch].reply, 0)<0){
				LOG(L_ERR, "ERROR: t_continue_reply: Error in run_top_route\n");
			}
			exec_post_script_cb(t->uac[branch].reply, cb_type);
		}

		LM_DBG("restoring previous environment");
		faked_env( t, 0, 1);

		/*lock transaction replies - will be unlocked when reply is relayed*/
		LOCK_REPLIES( t );
		if ( is_local(t) ) {
			LM_DBG("t is local - sending reply with status code: [%d]\n",
					t->uac[branch].reply->first_line.u.reply.statuscode);
			reply_status = local_reply( t, t->uac[branch].reply, branch,
					t->uac[branch].reply->first_line.u.reply.statuscode,
					&cancel_data );
			if (reply_status == RPS_COMPLETED) {
				/* no more UAC FR/RETR (if I received a 2xx, there may
				* be still pending branches ...
				*/
				cleanup_uac_timers( t );
				if (is_invite(t)) cancel_uacs(t, &cancel_data, F_CANCEL_B_KILL);
				/* There is no need to call set_final_timer because we know
				* that the transaction is local */
				put_on_wait(t);
			}else if (unlikely(cancel_data.cancel_bitmap)){
				/* cancel everything, even non-INVITEs (e.g in case of 6xx), use
				* cancel_b_method for canceling unreplied branches */
				cancel_uacs(t, &cancel_data, cfg_get(tm,tm_cfg, cancel_b_flags));
			}

		} else {
			LM_DBG("t is not local - relaying reply with status code: [%d]\n",
					t->uac[branch].reply->first_line.u.reply.statuscode);
			do_put_on_wait = 0;
			if(t->uac[branch].reply->first_line.u.reply.statuscode>=200){
				do_put_on_wait = 1;
			}
			reply_status=relay_reply( t, t->uac[branch].reply, branch,
					t->uac[branch].reply->first_line.u.reply.statuscode,
					&cancel_data, do_put_on_wait );
			if (reply_status == RPS_COMPLETED) {
				/* no more UAC FR/RETR (if I received a 2xx, there may
				be still pending branches ...
				*/
				cleanup_uac_timers( t );
				/* 2xx is a special case: we can have a COMPLETED request
				* with branches still open => we have to cancel them */
				if (is_invite(t) && cancel_data.cancel_bitmap) 
					cancel_uacs( t, &cancel_data,  F_CANCEL_B_KILL);
				/* FR for negative INVITES, WAIT anything else */
				/* Call to set_final_timer is embedded in relay_reply to avoid
				* race conditions when reply is sent out and an ACK to stop
				* retransmissions comes before retransmission timer is set.*/
			}else if (unlikely(cancel_data.cancel_bitmap)){
				/* cancel everything, even non-INVITEs (e.g in case of 6xx), use
				* cancel_b_method for canceling unreplied branches */
				cancel_uacs(t, &cancel_data, cfg_get(tm,tm_cfg, cancel_b_flags));
			}

		}
		t->uac[branch].request.flags|=F_RB_REPLIED;

		if (reply_status==RPS_ERROR){
			goto done;
		}

		/* update FR/RETR timers on provisional replies */

		msg_status=t->uac[branch].reply->REPLY_STATUS;
		last_uac_status=t->uac[branch].last_received;

		if (is_invite(t) && msg_status<200 &&
			( cfg_get(tm, tm_cfg, restart_fr_on_each_reply) ||
			( (last_uac_status<msg_status) &&
			((msg_status>=180) || (last_uac_status==0)) )
		) ) { /* provisional now */
			restart_rb_fr(& t->uac[branch].request, t->fr_inv_timeout);
			t->uac[branch].request.flags|=F_RB_FR_INV; /* mark fr_inv */
		}
            
	}

done:
	UNLOCK_ASYNC_CONTINUE(t);

	if(t->async_backup.backup_route != TM_ONREPLY_ROUTE){
		/* unref the transaction */
		t_unref(t->uas.request);
	} else {
		tm_ctx_set_branch_index(T_BR_UNDEFINED);        
		/* unref the transaction */
		t_unref(t->uac[branch].reply);
		LOG(L_DBG,"DEBUG: t_continue_reply: Freeing earlier cloned reply\n");

		/* free lumps that were added during reply processing */
		del_nonshm_lump( &(t->uac[branch].reply->add_rm) );
		del_nonshm_lump( &(t->uac[branch].reply->body_lumps) );
		del_nonshm_lump_rpl( &(t->uac[branch].reply->reply_lump) );

		/* free header's parsed structures that were added */
		for( hdr=t->uac[branch].reply->headers ; hdr ; hdr=hdr->next ) {
			if ( hdr->parsed && hdr_allocs_parse(hdr) &&
				(hdr->parsed<(void*)t->uac[branch].reply ||
				hdr->parsed>=(void*)t->uac[branch].end_reply)) {
				clean_hdr_field(hdr);
				hdr->parsed = 0;
			}
		}

		/* now go through hdr_fields themselves and remove the pkg allocated space */
		hdr = t->uac[branch].reply->headers;
		while (hdr) {
			if ( hdr && ((void*)hdr<(void*)t->uac[branch].reply ||
				(void*)hdr>=(void*)t->uac[branch].end_reply)) {
				//this header needs to be freed and removed form the list.
				if (!prev) {
					t->uac[branch].reply->headers = hdr->next;
				} else {
					prev->next = hdr->next;
				}
				tmp = hdr;
				hdr = hdr->next;
				pkg_free(tmp);
			} else {
				prev = hdr;
				hdr = hdr->next;
			}
		}
		sip_msg_free(t->uac[branch].reply);
		t->uac[branch].reply = 0;
	}
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

	if(t->async_backup.backup_route != TM_ONREPLY_ROUTE){
		t_unref(t->uas.request);
	} else {
		/* unref the transaction */
		t_unref(t->uac[branch].reply);
	}
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
        
	if(t->async_backup.backup_route != TM_ONREPLY_ROUTE){
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
	}else{
		branch = t->async_backup.backup_branch;

		LOG(L_DBG,"DEBUG: t_cancel_suspend_reply: This is a cancel suspend for a response\n");

		t->uac[branch].reply->msg_flags &= ~FL_RPL_SUSPENDED;
		if (t->uas.request) t->uas.request->msg_flags&= ~FL_RPL_SUSPENDED;
        }
	
	return 0;
}

