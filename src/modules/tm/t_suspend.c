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

#include "../../core/action.h"
#include "../../core/script_cb.h"
#include "../../core/dset.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/kemi.h"

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

#include "../../core/data_lump.h"
#include "../../core/data_lump_rpl.h"


#define LOCK_ASYNC_CONTINUE(_t) LOCK_REPLIES(_t)
#define UNLOCK_ASYNC_CONTINUE(_t) UNLOCK_REPLIES(_t)

/* Suspends the transaction for later use.
 * Save the returned hash_index and label to get
 * back to the SIP request processing, see the readme.
 *
 * Return value:
 * 	0  - success
 * 	<0 - failure
 */
int t_suspend(
		struct sip_msg *msg, unsigned int *hash_index, unsigned int *label)
{
	struct cell *t;
	int branch;
	int sip_msg_len;

	t = get_t();
	if(!t || t == T_UNDEFINED) {
		LM_ERR("transaction has not been created yet\n");
		return -1;
	}

	if(t->flags & T_CANCELED) {
		/* The transaction has already been canceled */
		LM_DBG("trying to suspend an already canceled transaction\n");
		ser_error = E_CANCELED;
		return 1;
	}
	if(t->uas.status >= 200) {
		LM_DBG("trasaction sent out a final response already - %d\n",
				t->uas.status);
		return -3;
	}

	if(msg->first_line.type != SIP_REPLY) {
		/* send a 100 Trying reply, because the INVITE processing
		will probably take a long time */
		if(msg->REQ_METHOD == METHOD_INVITE && (t->flags & T_AUTO_INV_100)
				&& (t->uas.status < 100)) {
			if(!t_reply(t, msg, 100, cfg_get(tm, tm_cfg, tm_auto_inv_100_r)))
				LM_DBG("suspending request processing - sending 100 reply\n");
		}

		if((t->nr_of_outgoings == 0) && /* if there had already been
			an UAC created, then the lumps were
			saved as well */
				save_msg_lumps(t->uas.request, msg)) {
			LM_ERR("failed to save the message lumps\n");
			return -1;
		}
		/* save the message flags */
		t->uas.request->flags = msg->flags;

		/* add a blind UAC to let the fr timer running */
		if(add_blind_uac() < 0) {
			LM_ERR("failed to add the blind UAC\n");
			return -1;
		}
		/* propagate failure route to new branch
		 * - failure route to be executed if the branch is not continued
		 *   before timeout */
		t->uac[t->async_backup.blind_uac].on_failure = t->on_failure;
		t->flags |= T_ASYNC_SUSPENDED;
	} else {
		LM_DBG("this is a suspend on reply - setting msg flag to SUSPEND\n");
		msg->msg_flags |= FL_RPL_SUSPENDED;
		/* this is a reply suspend find which branch */

		if(t_check(msg, &branch) == -1) {
			LM_ERR("failed find UAC branch\n");
			return -1;
		}

		if(!t->uac[branch].reply) {
			sip_msg_len = 0;
			LM_DBG("found a match with branch id [%d] - "
				   "cloning reply message to t->uac[branch].reply\n",
					branch);
			t->uac[branch].reply = sip_msg_cloner(msg, &sip_msg_len);

			if(!t->uac[branch].reply) {
				LM_ERR("can't alloc' clone memory\n");
				return -1;
			}
			t->uac[branch].end_reply =
					((char *)t->uac[branch].reply) + sip_msg_len;
		} else {
			LM_DBG("found a match with branch id [%d] - "
				   "message already cloned to t->uac[branch].reply\n",
					branch);
			// This can happen when suspending more than once in a reply.
		}
		LM_DBG("saving transaction data\n");
		t->uac[branch].reply->flags = msg->flags;
		t->flags |= T_ASYNC_SUSPENDED;
	}

	*hash_index = t->hash_index;
	*label = t->label;

	/* reset the continue flag to be able to suspend in a failure route */
	t->flags &= ~T_ASYNC_CONTINUE;

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
static int t_continue_helper(unsigned int hash_index, unsigned int label,
		struct action *rtact, str *cbname, str *cbparam, int skip_timer)
{
	tm_cell_t *t;
	tm_cell_t *backup_T = T_UNDEFINED;
	int backup_T_branch = T_BR_UNDEFINED;
	sip_msg_t *faked_req;
	sip_msg_t *brpl = NULL;
	void *erpl = NULL;
	int faked_req_len = 0;
	struct cancel_info cancel_data;
	int branch;
	struct ua_client *uac = NULL;
	int ret;
	int cb_type;
	int msg_status;
	int last_uac_status;
	int reply_status;
	int do_put_on_wait;
	struct hdr_field *hdr, *prev = 0, *tmp = 0;
	int route_type_bk;
	sr_kemi_eng_t *keng = NULL;
	str evname = str_init("tm:continue");

	cfg_update();

	backup_T = get_t();
	backup_T_branch = get_t_branch();

	if(t_lookup_ident_filter(&t, hash_index, label, skip_timer) < 0) {
		set_t(backup_T, backup_T_branch);
		LM_WARN("active transaction not found\n");
		return -1;
	}

	if(!(t->flags & T_ASYNC_SUSPENDED)) {
		LM_WARN("transaction is not suspended [%u:%u]\n", hash_index, label);
		set_t(backup_T, backup_T_branch);
		return -2;
	}

	if(t->flags & T_CANCELED) {
		t->flags &= ~T_ASYNC_SUSPENDED;
		/* The transaction has already been canceled,
		 * needless to continue */
		UNREF(t); /* t_unref would kill the transaction */
		/* reset T as we have no working T anymore */
		set_t(backup_T, backup_T_branch);
		return 1;
	}

	/* The transaction has to be locked to protect it
	 * form calling t_continue() multiple times simultaneously */
	LOCK_ASYNC_CONTINUE(t);

	t->flags |= T_ASYNC_CONTINUE; /* we can now know anywhere in kamailio
					 * that we are executing post a suspend */

	/* transaction is no longer suspended, resetting the SUSPEND flag */
	t->flags &= ~T_ASYNC_SUSPENDED;

	/* which route block type were we in when we were suspended */
	cb_type = FAILURE_CB_TYPE;
	switch(t->async_backup.backup_route) {
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

	if(t->async_backup.backup_route != TM_ONREPLY_ROUTE) {
		/* resume processing of a sip request */
		/* get the branch of the blind UAC setup during suspend */
		branch = t->async_backup.blind_uac;
		if(branch >= 0) {
			stop_rb_timers(&t->uac[branch].request);

			if(t->uac[branch].last_received != 0) {
				/* Either t_continue() has already been
				* called or the branch has already timed out.
				* Needless to continue. */
				t->flags &= ~T_ASYNC_CONTINUE;
				UNLOCK_ASYNC_CONTINUE(t);
				UNREF(t); /* t_unref would kill the transaction */
				set_t(backup_T, backup_T_branch);
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
			t->uac[branch].last_received = 500;
			if(t->uac[branch].reply != NULL) {
				LM_WARN("reply (%p) already set for suspended transaction"
						" (branch: %d)\n",
						t->uac[branch].reply, branch);
			} else {
				/* set it as a faked reply */
				t->uac[branch].reply = FAKED_REPLY;
			}
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
		faked_req = fake_req(
				t->uas.request, 0 /* extra flags */, uac, &faked_req_len);
		if(faked_req == NULL) {
			LM_ERR("building fake_req failed\n");
			ret = -1;
			goto kill_trans;
		}
		faked_env(t, faked_req, 1);

		route_type_bk = get_route_type();
		set_route_type(FAILURE_ROUTE);
		/* execute the pre/post -script callbacks based on original route block */
		if(exec_pre_script_cb(faked_req, cb_type) > 0) {
			if(rtact != NULL) {
				if(run_top_route(rtact, faked_req, 0) < 0) {
					LM_ERR("failure inside run_top_route\n");
				}
			} else {
				if(cbname != NULL && cbname->s != NULL) {
					keng = sr_kemi_eng_get();
					if(keng != NULL) {
						if(cbparam && cbparam->s) {
							evname = *cbparam;
						}
						if(sr_kemi_route(keng, faked_req, FAILURE_ROUTE, cbname,
								   &evname)
								< 0) {
							LM_ERR("error running event route kemi callback\n");
						}
					} else {
						LM_DBG("event callback (%.*s) set, but no cfg engine\n",
								cbname->len, cbname->s);
					}
				} else {
					LM_WARN("no continue callback\n");
				}
			}
			exec_post_script_cb(faked_req, cb_type);
		}
		set_route_type(route_type_bk);

		/* TODO: save_msg_lumps should clone the lumps to shm mem */

		/* restore original environment */
		faked_env(t, 0, 1);
		/* update the flags */
		t->uas.request->flags = faked_req->flags;
		/* free the fake msg */
		free_faked_req(faked_req, faked_req_len);


		if(t->uas.status < 200) {
			/* No final reply has been sent yet.
			* Check whether or not there is any pending branch.
			*/
			for(branch = 0; branch < t->nr_of_outgoings; branch++) {
				if(t->uac[branch].last_received < 200)
					break;
			}

			if(branch == t->nr_of_outgoings) {
				/* There is not any open branch so there is
			 * no chance that a final response will be received. */
				ret = 0;
				goto kill_trans;
			}
		}

	} else {
		/* resume processing of a sip response */
		branch = t->async_backup.backup_branch;

		init_cancel_info(&cancel_data);

		LM_DBG("continuing from a suspended reply"
			   " - resetting the suspend branch flag\n");

		if(t->uac[branch].reply) {
			t->uac[branch].reply->msg_flags &= ~FL_RPL_SUSPENDED;
		} else {
			LM_WARN("no reply in t_continue for branch. not much we can do\n");
			return 0;
		}
		if(t->uas.request)
			t->uas.request->msg_flags &= ~FL_RPL_SUSPENDED;

		faked_env(t, t->uac[branch].reply, 1);

		if(exec_pre_script_cb(t->uac[branch].reply, cb_type) > 0) {
			if(rtact != NULL) {
				if(run_top_route(rtact, t->uac[branch].reply, 0) < 0) {
					LM_ERR("Error in run_top_route\n");
				}
			} else {
				if(cbname != NULL && cbname->s != NULL) {
					keng = sr_kemi_eng_get();
					if(keng != NULL) {
						if(cbparam && cbparam->s) {
							evname = *cbparam;
						}
						if(sr_kemi_route(keng, t->uac[branch].reply,
								   TM_ONREPLY_ROUTE, cbname, &evname)
								< 0) {
							LM_ERR("error running event route kemi callback\n");
						}
					} else {
						LM_DBG("event callback (%.*s) set, but no cfg engine\n",
								cbname->len, cbname->s);
					}
				} else {
					LM_WARN("no continue callback\n");
				}
			}
			exec_post_script_cb(t->uac[branch].reply, cb_type);
		}

		LM_DBG("restoring previous environment\n");
		faked_env(t, 0, 1);

		if(t->flags & T_ASYNC_SUSPENDED) {
			LM_DBG("The transaction is suspended, so not continuing\n");
			t->flags &= ~T_ASYNC_CONTINUE;
			UNLOCK_ASYNC_CONTINUE(t);
			set_t(backup_T, backup_T_branch);
			return 0;
		}

		/*lock transaction replies - will be unlocked when reply is relayed*/
		LOCK_REPLIES(t);
		if(is_local(t)) {
			LM_DBG("t is local - sending reply with status code: [%d]\n",
					t->uac[branch].reply->first_line.u.reply.statuscode);
			reply_status = local_reply(t, t->uac[branch].reply, branch,
					t->uac[branch].reply->first_line.u.reply.statuscode,
					&cancel_data);
			if(reply_status == RPS_COMPLETED) {
				/* no more UAC FR/RETR (if I received a 2xx, there may
				* be still pending branches ...
				*/
				cleanup_uac_timers(t);
				if(is_invite(t))
					cancel_uacs(t, &cancel_data, F_CANCEL_B_KILL);
				/* There is no need to call set_final_timer because we know
				* that the transaction is local */
				put_on_wait(t);
			} else if(unlikely(cancel_data.cancel_bitmap)) {
				/* cancel everything, even non-INVITEs (e.g in case of 6xx), use
				* cancel_b_method for canceling unreplied branches */
				cancel_uacs(
						t, &cancel_data, cfg_get(tm, tm_cfg, cancel_b_flags));
			}

		} else {
			LM_DBG("t is not local - relaying reply with status code: [%d]\n",
					t->uac[branch].reply->first_line.u.reply.statuscode);
			do_put_on_wait = 0;
			if(t->uac[branch].reply->first_line.u.reply.statuscode >= 200) {
				do_put_on_wait = 1;
			}
			reply_status = relay_reply(t, t->uac[branch].reply, branch,
					t->uac[branch].reply->first_line.u.reply.statuscode,
					&cancel_data, do_put_on_wait);
			if(reply_status == RPS_COMPLETED) {
				/* no more UAC FR/RETR (if I received a 2xx, there may
				be still pending branches ...
				*/
				cleanup_uac_timers(t);
				/* 2xx is a special case: we can have a COMPLETED request
				* with branches still open => we have to cancel them */
				if(is_invite(t) && cancel_data.cancel_bitmap)
					cancel_uacs(t, &cancel_data, F_CANCEL_B_KILL);
				/* FR for negative INVITES, WAIT anything else */
				/* Call to set_final_timer is embedded in relay_reply to avoid
				* race conditions when reply is sent out and an ACK to stop
				* retransmissions comes before retransmission timer is set.*/
			} else if(unlikely(cancel_data.cancel_bitmap)) {
				/* cancel everything, even non-INVITEs (e.g in case of 6xx), use
				* cancel_b_method for canceling unreplied branches */
				cancel_uacs(
						t, &cancel_data, cfg_get(tm, tm_cfg, cancel_b_flags));
			}
		}
		t->uac[branch].request.flags |= F_RB_REPLIED;

		if(reply_status == RPS_ERROR) {
			goto done;
		}

		/* update FR/RETR timers on provisional replies */

		msg_status = t->uac[branch].reply->REPLY_STATUS;
		last_uac_status = t->uac[branch].last_received;

		if(is_invite(t) && msg_status < 200
				&& (cfg_get(tm, tm_cfg, restart_fr_on_each_reply)
						|| ((last_uac_status < msg_status)
								&& ((msg_status >= 180)
										|| (last_uac_status
												== 0))))) { /* provisional now */
#ifdef TIMER_DEBUG
			LM_DBG("updating FR/RETR timers, \"fr_inv_timeout\": %d\n",
					t->fr_inv_timeout);
#endif
			restart_rb_fr(&t->uac[branch].request, t->fr_inv_timeout);
			t->uac[branch].request.flags |= F_RB_FR_INV; /* mark fr_inv */
		}
	}

done:
	t->flags &= ~T_ASYNC_CONTINUE;
	if(t->async_backup.backup_route == TM_ONREPLY_ROUTE) {
		/* response handling */
		/* backup branch reply to free it later and reset it here under lock */
		brpl = t->uac[branch].reply;
		erpl = (void *)t->uac[branch].end_reply;
		t->uac[branch].reply = 0;
		t->uac[branch].end_reply = 0;
	}
	UNLOCK_ASYNC_CONTINUE(t);

	if(t->async_backup.backup_route != TM_ONREPLY_ROUTE) {
		/* request handling */
		/* unref the transaction */
		t_unref(t->uas.request);
	} else {
		/* response handling */
		tm_ctx_set_branch_index(T_BR_UNDEFINED);
		if(brpl) {
			/* unref the transaction */
			t_unref(brpl);
			LM_DBG("Freeing earlier cloned reply\n");

			/* free lumps that were added during reply processing */
			del_nonshm_lump(&(brpl->add_rm));
			del_nonshm_lump(&(brpl->body_lumps));
			del_nonshm_lump_rpl(&(brpl->reply_lump));

			/* free header's parsed structures that were added */
			for(hdr = brpl->headers; hdr; hdr = hdr->next) {
				if(hdr->parsed && hdr_allocs_parse(hdr)
						&& (hdr->parsed < (void *)brpl
								|| (erpl && hdr->parsed >= (void *)erpl))) {
					clean_hdr_field(hdr);
					hdr->parsed = 0;
				}
			}

			/* now go through hdr fields themselves
			 * and remove the pkg allocated space */
			hdr = brpl->headers;
			while(hdr) {
				if(hdr
						&& ((void *)hdr < (void *)brpl
								|| (void *)hdr >= (void *)erpl)) {
					/* this header needs to be freed and removed form the list */
					if(!prev) {
						brpl->headers = hdr->next;
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

			/* trigger free of body */
			if(brpl->body && brpl->body->free) {
				brpl->body->free(&brpl->body);
			}
			sip_msg_free(brpl);
		}
	}

	set_t(backup_T, backup_T_branch);
	return 0;

kill_trans:
	/* The script has hopefully set the error code. If not,
	 * let us reply with a default error. */
	if((kill_transaction_unsafe(t, tm_error ? tm_error : E_UNSPEC)) <= 0) {
		LM_ERR("reply generation failed\n");
		/* The transaction must be explicitely released,
		 * no more timer is running */
		t->flags &= ~T_ASYNC_CONTINUE;
		UNLOCK_ASYNC_CONTINUE(t);
		t_release_transaction(t);
	} else {
		t->flags &= ~T_ASYNC_CONTINUE;
		UNLOCK_ASYNC_CONTINUE(t);
	}

	/* unref the transaction */
	if(t->async_backup.backup_route != TM_ONREPLY_ROUTE) {
		/* request handling */
		t_unref(t->uas.request);
	} else {
		/* response handling */
		t_unref(t->uac[branch].reply);
	}
	set_t(backup_T, backup_T_branch);
	return ret;
}

int t_continue(
		unsigned int hash_index, unsigned int label, struct action *route)
{
	return t_continue_helper(hash_index, label, route, NULL, NULL, 1);
}
int t_continue_skip_timer(
		unsigned int hash_index, unsigned int label, struct action *route)
{
	return t_continue_helper(hash_index, label, route, NULL, NULL, 0);
}

int t_continue_cb(
		unsigned int hash_index, unsigned int label, str *cbname, str *cbparam)
{
	return t_continue_helper(hash_index, label, NULL, cbname, cbparam, 0);
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
	struct cell *t;
	int branch;

	t = get_t();
	if(!t || t == T_UNDEFINED) {
		LM_ERR("no active transaction\n");
		return -1;
	}
	/* Only to double-check the IDs */
	if((t->hash_index != hash_index) || (t->label != label)) {
		LM_ERR("transaction id mismatch\n");
		return -1;
	}

	if(t->async_backup.backup_route != TM_ONREPLY_ROUTE) {
		/* The transaction does not need to be locked because this
		* function is either executed from the original route block
		* or from failure route which already locks */

		reset_kr(); /* the blind UAC of t_suspend has set kr */

		/* Try to find the blind UAC, and cancel its fr timer.
		* We assume that the last blind uac called this function. */
		for(branch = t->nr_of_outgoings - 1;
				branch >= 0 && t->uac[branch].request.buffer; branch--)
			;

		if(branch >= 0) {
			stop_rb_timers(&t->uac[branch].request);
			/* Set last_received to something >= 200,
			* the actual value does not matter, the branch
			* will never be picked up for response forwarding.
			* If last_received is lower than 200,
			* then the branch may tried to be cancelled later,
			* for example when t_reply() is called from
			* a failure rute => deadlock, because both
			* of them need the reply lock to be held. */
			t->uac[branch].last_received = 500;
		} else {
			/* Not a huge problem, fr timer will fire, but CANCEL
			will not be sent. last_received will be set to 408. */
			return -1;
		}
	} else {
		branch = t->async_backup.backup_branch;

		LM_DBG("This is a cancel suspend for a response\n");

		t->uac[branch].reply->msg_flags &= ~FL_RPL_SUSPENDED;
		if(t->uas.request)
			t->uas.request->msg_flags &= ~FL_RPL_SUSPENDED;
	}

	return 0;
}
