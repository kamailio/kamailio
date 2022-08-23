/*
 * late branching functions
 *
 * Copyright (C) 2014 Federico Cabiddu
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/route.h"
#include "../../core/data_lump.h"
#include "../../core/counters.h"
#include "../../core/dset.h"
#include "../../core/script_cb.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/contact/parse_contact.h"
#include "t_msgbuilder.h"
#include "t_lookup.h"
#include "t_fwd.h"
#include "t_reply.h"
#include "t_append_branches.h"

/* this function can act in two ways:
 * - first way, create branches for all existing location records
 *   of this particular AOR. Search for locations is done in
 *   the location table.
 * - second way, if the contact parameter is given,
 *   then only a desired location is meant for appending,
 *   if not found in the location table, an append will not happen
 *   for this AOR.
 *
 *   If the contact parameter is given, it must be of syntax:
 *     sip:<user>@<host>:<port>   (without parameters) */
int t_append_branches(str * contact) {
	struct cell *t = NULL;
	struct sip_msg *orig_msg = NULL;
	struct sip_msg *faked_req;
	int faked_req_len = 0;

	short outgoings;

	int success_branch;

	str current_uri;
	str dst_uri, path, instance, ruid, location_ua;
	struct socket_info* si;
	int q, i, found, append;
	flag_t backup_bflags = 0;
	flag_t bflags = 0;
	int new_branch, branch_ret, lowest_ret;
	branch_bm_t	added_branches;
	int replies_locked = 0;
	int ret = 0;

	t = get_t();
	if(t == NULL)
	{
		LM_ERR("cannot get transaction\n");
		return -1;
	}

	LM_DBG("transaction %u:%u in status %d\n", t->hash_index, t->label, t->uas.status);

	/* test if transaction has already been canceled */
	if (t->flags & T_CANCELED) {
		ser_error=E_CANCELED;
		return -1;
	}

	if ((t->uas.status >= 200 && t->uas.status<=399)
			|| ((t->uas.status >= 600 && t->uas.status)
				&& !(t->flags & (T_6xx | T_DISABLE_6xx))) ) {
		LM_DBG("transaction %u:%u in status %d: cannot append new branch\n",
				t->hash_index, t->label, t->uas.status);
		return -1;
	}

	/* set the lock on the transaction here */
	LOCK_REPLIES(t);
	replies_locked = 1;
	outgoings = t->nr_of_outgoings;
	orig_msg = t->uas.request;

	LM_DBG("Call %.*s: %d (%d) outgoing branches\n",orig_msg->callid->body.len,
			orig_msg->callid->body.s,outgoings, nr_branches);

	lowest_ret=E_UNSPEC;
	added_branches=0;

	/* it's a "late" branch so the on_branch variable has already been
	reset by previous execution of t_forward_nonack: we use the saved
	   value  */
	if (t->on_branch_delayed) {
		/* tell add_uac that it should run branch route actions */
		set_branch_route(t->on_branch_delayed);
	}
	faked_req = fake_req(orig_msg, 0, NULL,	&faked_req_len);
	if (faked_req==NULL) {
		LM_ERR("fake_req failed\n");
		return -1;
	}

	/* fake also the env. conforming to the fake msg */
	faked_env( t, faked_req, 0);

	/* DONE with faking ;-) -> run the failure handlers */
	init_branch_iterator();

	while((current_uri.s=next_branch( &current_uri.len, &q, &dst_uri, &path,
										&bflags, &si, &ruid, &instance, &location_ua))) {
		LM_DBG("Current uri %.*s\n",current_uri.len, current_uri.s);

		/* if the contact parameter is given, then append by
			an exact location that has been requested for this function call */
		if (contact->s != NULL && contact->len != 0) {

			LM_DBG("Comparing requested contact <%.*s> against location <%.*s>\n",
							contact->len, contact->s, current_uri.len, current_uri.s);

			append = 1;
			if (strstr(current_uri.s, contact->s) == NULL) {
				append = 0; /* this while cycle will be stopped */
			}

			/* do not append the branch if a contact does not match */
			if (!append)
				continue;

			LM_DBG("Branch will be appended for contact <%.*s>\n", contact->len, contact->s);
		}

		found = 0;
		for (i=0; i<outgoings; i++) {
			if (t->uac[i].ruid.len == ruid.len
					&& !memcmp(t->uac[i].ruid.s, ruid.s, ruid.len)
					&& t->uac[i].uri.len == current_uri.len
					&& !memcmp(t->uac[i].uri.s, current_uri.s, current_uri.len)) {
				LM_DBG("branch already added [%.*s]\n", ruid.len, ruid.s);
				found = 1;
				break;
			}
		}
		if (found)
			continue;

		setbflagsval(0, bflags);
		new_branch=add_uac( t, faked_req, &current_uri,
					(dst_uri.len) ? (&dst_uri) : &current_uri,
					&path, 0, si, faked_req->fwd_send_flags,
					PROTO_NONE, (dst_uri.len)?0:UAC_SKIP_BR_DST_F, &instance,
					&ruid, &location_ua);

		LM_DBG("added branch [%.*s] with ruid [%.*s]\n",
				current_uri.len, current_uri.s, ruid.len, ruid.s);

		/* test if cancel was received meanwhile */
		if (t->flags & T_CANCELED) goto canceled;

		if (new_branch>=0)
			added_branches |= 1<<new_branch;
		else
			lowest_ret=MIN_int(lowest_ret, new_branch);
	}

	clear_branches();

	LM_DBG("Call %.*s: %d (%d) outgoing branches after clear_branches()\n",
			orig_msg->callid->body.len, orig_msg->callid->body.s,outgoings, nr_branches);
	setbflagsval(0, backup_bflags);

	/* update message flags, if changed in branch route */
	t->uas.request->flags = faked_req->flags;

	if (added_branches==0) {
		if(lowest_ret!=E_CFG)
			LM_ERR("failure to add branches (%d)\n", lowest_ret);
		ser_error=lowest_ret;
		ret = lowest_ret;
		goto done;
	}

	ser_error=0; /* clear branch adding errors */
	/* send them out now */
	success_branch=0;
	/* since t_append_branch can only be called from REQUEST_ROUTE, always lock replies */

	for (i=outgoings; i<t->nr_of_outgoings; i++) {
		if (added_branches & (1<<i)) {
			branch_ret=t_send_branch(t, i, faked_req , 0, 0 /* replies are already locked */ );
			if (branch_ret>=0){ /* some kind of success */
				if (branch_ret==i) { /* success */
					success_branch++;
					if (unlikely(has_tran_tmcbs(t, TMCB_REQUEST_OUT)))
						run_trans_callbacks_with_buf( TMCB_REQUEST_OUT,
								&t->uac[nr_branches].request,
								faked_req, 0, TMCB_NONE_F);
				}
				else /* new branch added */
					added_branches |= 1<<branch_ret;
			}
		}
	}
	if (success_branch<=0) {
		/* return always E_SEND for now
		 * (the real reason could be: denied by onsend routes, blocklisted,
		 *  send failed or any of the errors listed before + dns failed
		 *  when attempting dns failover) */
		ser_error=E_SEND;
		/* else return the last error (?) */
		ret = -1;
		goto done;
	}

	ser_error=0; /* clear branch send errors, we have overall success */
	set_kr(REQ_FWDED);
	ret = success_branch;
	goto done;

canceled:
	LM_DBG("cannot append branches to a canceled transaction\n");
	/* reset processed branches */
	clear_branches();
	/* restore backup flags from initial env */
	setbflagsval(0, backup_bflags);
	/* update message flags, if changed in branch route */
	t->uas.request->flags = faked_req->flags;
	/* if needed unlock transaction's replies */
		/* restore the number of outgoing branches
		 * since new branches have not been completed */
	t->nr_of_outgoings = outgoings;
	ser_error=E_CANCELED;
	ret = -1;
done:
	/* restore original environment and free the fake msg */
	faked_env( t, 0, 0);
	free_faked_req(faked_req, faked_req_len);

	if (likely(replies_locked)) {
		replies_locked = 0;
		UNLOCK_REPLIES(t);
	}
	return ret;
}
