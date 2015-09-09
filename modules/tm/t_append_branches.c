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

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../route.h"
#include "../../data_lump.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../dset.h"
#include "../../script_cb.h"
#include "../../parser/msg_parser.h"
#include "../../parser/contact/parse_contact.h"
#include "t_msgbuilder.h"
#include "t_lookup.h"
#include "t_fwd.h"
#include "t_reply.h"
#include "t_append_branches.h"

int t_append_branches(void) {
	struct cell *t = NULL;
	struct sip_msg *orig_msg = NULL;
	short outgoings;

	int success_branch;

	str current_uri;
	str dst_uri, path, instance, ruid, location_ua;
	struct socket_info* si;
	int q, i, found;
	flag_t backup_bflags = 0;
	flag_t bflags = 0;
	int new_branch, branch_ret, lowest_ret;
	branch_bm_t	added_branches;
	int replies_locked = 0;

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

	init_branch_iterator();

	while((current_uri.s=next_branch( &current_uri.len, &q, &dst_uri, &path,
										&bflags, &si, &ruid, &instance, &location_ua))) {
		LM_DBG("Current uri %.*s\n",current_uri.len, current_uri.s);

		found = 0;
		for (i=0; i<outgoings; i++) {
			if (t->uac[i].ruid.len == ruid.len
					&& !memcmp(t->uac[i].ruid.s, ruid.s, ruid.len)) {
				LM_DBG("branch already added [%.*s]\n", ruid.len, ruid.s);
				found = 1;
				break;
			}
		}
		if (found)
			continue;

		setbflagsval(0, bflags);
		new_branch=add_uac( t, orig_msg, &current_uri,
					(dst_uri.len) ? (&dst_uri) : &current_uri,
					&path, 0, si, orig_msg->fwd_send_flags,
					orig_msg->rcv.proto, (dst_uri.len)?-1:UAC_SKIP_BR_DST_F, &instance,
					&ruid, &location_ua);
		
		LM_DBG("added branch [%.*s] with ruid [%.*s]\n", current_uri.len, current_uri.s, ruid.len, ruid.s);

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
	t->uas.request->flags = orig_msg->flags;

	if (added_branches==0) {
		if(lowest_ret!=E_CFG)
			LOG(L_ERR, "ERROR: t_append_branch: failure to add branches\n");
		ser_error=lowest_ret;
		replies_locked = 0;
		UNLOCK_REPLIES(t);
		return lowest_ret;
	}

	ser_error=0; /* clear branch adding errors */
	/* send them out now */
	success_branch=0;
	/* since t_append_branch can only be called from REQUEST_ROUTE, always lock replies */

	for (i=outgoings; i<t->nr_of_outgoings; i++) {
		if (added_branches & (1<<i)) {
			branch_ret=t_send_branch(t, i, orig_msg , 0, 0 /* replies are already locked */ );
			if (branch_ret>=0){ /* some kind of success */
				if (branch_ret==i) { /* success */
					success_branch++;
					if (unlikely(has_tran_tmcbs(t, TMCB_REQUEST_OUT)))
						run_trans_callbacks_with_buf( TMCB_REQUEST_OUT,
								&t->uac[nr_branches].request,
								orig_msg, 0, -orig_msg->REQ_METHOD);
				}
				else /* new branch added */
					added_branches |= 1<<branch_ret;
			}
		}
	}
	if (success_branch<=0) {
		/* return always E_SEND for now
		 * (the real reason could be: denied by onsend routes, blacklisted,
		 *  send failed or any of the errors listed before + dns failed
		 *  when attempting dns failover) */
		ser_error=E_SEND;
		/* else return the last error (?) */
		/* the caller should take care and delete the transaction */
		replies_locked = 0;
		UNLOCK_REPLIES(t);
		return -1;
	}

	ser_error=0; /* clear branch send errors, we have overall success */
	set_kr(REQ_FWDED);
	replies_locked = 0;
	UNLOCK_REPLIES(t);
	return 1;

canceled:
	DBG("t_append_branches: cannot append branches to a canceled transaction\n");
	/* reset processed branches */
	clear_branches();
	/* restore backup flags from initial env */
	setbflagsval(0, backup_bflags);
	/* update message flags, if changed in branch route */
	t->uas.request->flags = orig_msg->flags;
	/* if needed unlock transaction's replies */
	if (likely(replies_locked)) {
		/* restore the number of outgoing branches
		 * since new branches have not been completed */
		t->nr_of_outgoings = outgoings;
		replies_locked = 0;
		UNLOCK_REPLIES(t);
	}
	ser_error=E_CANCELED;
	return -1;
}
