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
 */


#ifndef _CANCEL_H
#define _CANCEL_H

/* a buffer is empty but cannot be used by anyone else;
   particularly, we use this value in the buffer pointer
   in local_buffer to tell "a process is already scheduled
   to generate a CANCEL, other processes are not supposed to"
   (which might happen if for example in a three-branch forking,
   two 200 would enter separate processes and compete for
   cancelling the third branch); note that to really avoid
   race conditions, the value must be set in REPLY_LOCK
*/

#define BUSY_BUFFER ((char *)-1)

void which_cancel( struct cell *t, branch_bm_t *cancel_bm );
void cancel_uacs( struct cell *t, branch_bm_t cancel_bm );
void cancel_branch( struct cell *t, int branch );

char *build_cancel(struct cell *Trans,unsigned int branch,
	unsigned int *len );

inline short static should_cancel_branch( struct cell *t, int b )
{
	int last_received;
	short should;

	last_received=t->uac[b].last_received;
	/* cancel only if provisional received and noone else
	   attempted to cancel yet */
	should=last_received>=100 && last_received<200
		&& t->uac[b].local_cancel.buffer==0;
	/* we'll cancel -- label it so that noone else
		(e.g. another 200 branch) will try to do the same */
	if (should) t->uac[b].local_cancel.buffer=BUSY_BUFFER;
	return should;
}


#endif
