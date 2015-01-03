/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifndef _CANCEL_H
#define _CANCEL_H

#include <stdio.h> /* just for FILE* for fifo_uac_cancel */
#include "../../rpc.h"
#include "../../atomic_ops.h"
#include "defs.h"
#include "h_table.h"
#include "t_reply.h"


/* a buffer is empty but cannot be used by anyone else;
   particularly, we use this value in the buffer pointer
   in local_buffer to tell "a process is already scheduled
   to generate a CANCEL, other processes are not supposed to"
   (which might happen if for example in a three-branch forking,
   two 200 would enter separate processes and compete for
   canceling the third branch); note that to really avoid
   race conditions, the value must be set in REPLY_LOCK
*/

#define BUSY_BUFFER ((char *)-1)

/* flags for cancel_uacs(), cancel_branch() */
#define F_CANCEL_B_KILL 1  /*  will completely stop the  branch (stops the
							   timers), use only before a put_on_wait()
							   or set_final_timer()*/
#define F_CANCEL_B_FAKE_REPLY 2 /* will send a fake 487 to all branches that
								 haven't received any response (>=100). It
								 assumes the REPLY_LOCK is not held (if it is
								 => deadlock) */
#define F_CANCEL_B_FORCE_C 4 /* will send a cancel even if no reply was 
								received; F_CANCEL_B_FAKE_REPLY will be 
								ignored */
#define F_CANCEL_B_FORCE_RETR 8  /* will not stop request retr. on a branch
									if no provisional response was received;
									F_CANCEL_B_FORCE_C, F_CANCEL_B_FAKE_REPLY
									and F_CANCE_B_KILL take precedence */
#define F_CANCEL_UNREF 16 /* unref the trans after canceling */


void prepare_to_cancel(struct cell *t, branch_bm_t *cancel_bm, branch_bm_t s);
int cancel_uacs( struct cell *t, struct cancel_info* cancel_data, int flags );
int cancel_all_uacs(struct cell *trans, int how);
int cancel_branch( struct cell *t, int branch,
#ifdef CANCEL_REASON_SUPPORT
					struct cancel_reason* reason,
#endif /* CANCEL_REASON_SUPPORT */
					int flags );

typedef int(*cancel_uacs_f)(struct cell *t, struct cancel_info* cancel_data,
		int flags);
typedef int (*cancel_all_uacs_f)(struct cell *trans, int how);

typedef void (*prepare_to_cancel_f)(struct cell *t, branch_bm_t *cancel_bm,
									branch_bm_t skip_branches);


/** Check if one branch needs CANCEL-ing and prepare it if it does.
 * Can be called w/o REPLY_LOCK held
 *  between this call and the call to cancel_uacs()/cancel_branch()
 *  if noreply is set to 1 it will return true even if no reply was received
 *   (it will return false only if a final response or a cancel have already 
 *    been sent on the current branch).
 *  if noreply is set to 0 it will return true only if no cancel has already 
 *   been sent and a provisional (<200) reply >=100 was received.
 *  WARNING: has side effects: marks branches that should be canceled
 *   and a second call won't return them again.
 * @param t - transaction
 * @param b - branch number
 * @param noreply - 0 or 1. If 1 it will consider a branch with no replies
 *  received so far as cancel-able. If 0 only branches that have received
 * a provisional reply (>=100 <200) will be considered.
 *
 * @return 1 if the branch must be canceled (it will be internally marked as
 *  cancel-in-progress) and 0 if it doesn't (either a CANCEL is not needed or a
 *  CANCEL is in progress: somebody else is trying to CANCEL in the same time).
 */
inline short static prepare_cancel_branch( struct cell *t, int b, int noreply )
{
	int last_received;
	unsigned long old;

	last_received=t->uac[b].last_received;
	/* if noreply=1 cancel even if no reply received (in this case 
	 * cancel_branch()  won't actually send the cancel but it will do the 
	 * cleanup) */
	if (last_received<200 && (noreply || last_received>=100)){
		old=atomic_cmpxchg_long((void*)&t->uac[b].local_cancel.buffer, 0,
									(long)(BUSY_BUFFER));
		return old==0;
	}
	return 0;
}

void rpc_cancel(rpc_t* rpc, void* c);
int cancel_b_flags_fixup(void* handle, str* gname, str* name, void** val);
int cancel_b_flags_get(unsigned int* f, int m);

typedef unsigned int (*tuaccancel_f)( str *headers,str *body,
		unsigned int cancelledIdx,unsigned int cancelledLabel,
		transaction_cb cb, void* cbp);

unsigned int t_uac_cancel(str *headers,str *body,
		unsigned int cancelledIdx,unsigned int cancelledLabel,
		transaction_cb cb, void* cbp);

#endif
