/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * ---------
 *  2004-02-11  FIFO/CANCEL + alignments (hash=f(callid,cseq)) (uli+jiri)
 *  2006-10-10  should_cancel_branch() returns true even for branches with
 *               no response or with response <100 (andrei)
 */


#ifndef _CANCEL_H
#define _CANCEL_H

#include <stdio.h> /* just for FILE* for fifo_uac_cancel */
#include "../../rpc.h"
#include "defs.h"


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


void which_cancel( struct cell *t, branch_bm_t *cancel_bm );
int cancel_uacs( struct cell *t, branch_bm_t cancel_bm, int flags );
int cancel_branch( struct cell *t, int branch, int flags );

typedef int(*cancel_uacs_f)( struct cell *t, branch_bm_t cancel_bm,
								int flags );



/* should be called either with the REPLY_LOCK held or if its known
 *  by other means that no other cancel trigerring reply can appear
 *  between this call and the call to cancel_uacs()/cancel_branch()
 *  WARNING: has side effects: marks branches that should be canceled
 *   and a second call won't return them again */
inline short static should_cancel_branch( struct cell *t, int b )
{
	int last_received;
	short should;

	last_received=t->uac[b].last_received;
	/* cancel even if no reply received (in this case cancel_branch()
	 *  won't actually send the cancel but it will do the cleanup) */
	should=last_received<200 && t->uac[b].local_cancel.buffer==0;
	/* we'll cancel -- label it so that noone else
		(e.g. another 200 branch) will try to do the same */
	if (should) t->uac[b].local_cancel.buffer=BUSY_BUFFER;
	return should;
}

const char* rpc_cancel_doc[2];
void rpc_cancel(rpc_t* rpc, void* c);

#endif
