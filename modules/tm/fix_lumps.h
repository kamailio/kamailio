/*
 * $Id$
 *
 * here, we delete message lumps which are generated in
 * core functions using pkg_malloc and applied to shmem
 * requests; not doing so would result ugly memory problems
 *
 * I admit it is not a nice hack; -jiri 
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
/*
 * Histoy:
 * -------
 *  2003-11-24  changed free_via_lump to free_via_clen_lump and make it
 *              handle CONTENTLENGTH lumps also (andrei)
 */



#ifndef _FIX_LUMPS_H
#define _FIX_LUMPS_H

#include "defs.h"


/* used to delete attached via lumps from msg; msg can
   be either an original pkg msg, whose Via lump I want
   to delete before generating next branch, or a shmem-stored
   message processed during on_reply -- then I want to
   delete the Via lump for the same reason

   the other case when I want to delete them is when a message
   is stored in shmem for branch picking, forwarded lated and
   Via removal is applied to the shmem-ed message

   the same thing for Content-Length lumps (FIXME: this
   should be done in a nicer way)
*/
inline static void free_via_clen_lump( struct lump **list )
{
	struct lump *prev_lump, *lump, *a, *foo, *next;

	next=0;
	prev_lump=0;
	for(lump=*list;lump;lump=next) {
		next=lump->next;
		if (lump->type==HDR_VIA||lump->type==HDR_CONTENTLENGTH) {
			a=lump->before;
			while(a) {
				foo=a; a=a->before;
				free_lump(foo);
				pkg_free(foo);
			}
			a=lump->after;
			while(a) {
				foo=a; a=a->after;
				free_lump(foo);
				pkg_free(foo);
			}
			if (prev_lump) prev_lump->next = lump->next;
			else *list = lump->next;
			free_lump(lump);pkg_free(lump);
		} else {
			/* store previous position */
			prev_lump=lump;
		}
	}
}

#endif
