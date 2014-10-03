/*
 * $Id$
 *
 * here, we delete message lumps which are generated in
 * core functions using pkg_malloc and applied to shmem
 * requests; not doing so would result ugly memory problems
 *
 * I admit it is not a nice hack; -jiri 
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
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*
 * History:
 * -------
 *  2003-11-24  changed free_via_lump to free_via_clen_lump and make it
 *              handle CONTENTLENGTH lumps also (andrei)
 *  2005-07-04  lumps in SHM or dup'ed lumps are not freed and an warning
 *               message is logged (temporary fix) (andrei)
 */



#ifndef _FIX_LUMPS_H
#define _FIX_LUMPS_H



/** @brief used to delete attached via lumps from msg;

   msg can be either an original pkg msg, whose Via lump I want
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
		if (lump->type==HDR_VIA_T||lump->type==HDR_CONTENTLENGTH_T) {
			if (lump->flags & (LUMPFLAG_DUPED|LUMPFLAG_SHMEM)){
				LM_CRIT("free_via_clen_lmp: lump %p, flags %x\n",
						lump, lump->flags);
				/* ty to continue */
			}
			a=lump->before;
			while(a) {
				foo=a; a=a->before;
				if (!(foo->flags&(LUMPFLAG_DUPED|LUMPFLAG_SHMEM)))
					free_lump(foo);
				if (!(foo->flags&LUMPFLAG_SHMEM))
					pkg_free(foo);
			}
			a=lump->after;
			while(a) {
				foo=a; a=a->after;
				if (!(foo->flags&(LUMPFLAG_DUPED|LUMPFLAG_SHMEM)))
					free_lump(foo);
				if (!(foo->flags&LUMPFLAG_SHMEM))
					pkg_free(foo);
			}
			if (prev_lump) prev_lump->next = lump->next;
			else *list = lump->next;
			if (!(lump->flags&(LUMPFLAG_DUPED|LUMPFLAG_SHMEM)))
				free_lump(lump);
			if (!(lump->flags&LUMPFLAG_SHMEM))
				pkg_free(lump);
		} else {
			/* store previous position */
			prev_lump=lump;
		}
	}
}

#endif
