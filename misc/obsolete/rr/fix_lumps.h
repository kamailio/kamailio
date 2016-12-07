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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * History:
 * ........
 *  2006-04-19: copy-pasted form TM module (Miklos)
 */

#ifndef _FIX_LUMPS_H
#define _FIX_LUMPS_H


inline static void free_rr_lump( struct lump **list )
{
	struct lump *prev_lump, *lump, *a, *foo, *next;
	int first_shmem;

	first_shmem=1;
	next=0;
	prev_lump=0;
	for(lump=*list;lump;lump=next) {
		next=lump->next;
		if (lump->type==HDR_RECORDROUTE_T) {
			/* may be called from railure_route */
			/* if (lump->flags & (LUMPFLAG_DUPED|LUMPFLAG_SHMEM)){
				LOG(L_CRIT, "BUG: free_rr_lmp: lump %p, flags %x\n",
						lump, lump->flags);
			*/	/* ty to continue */
			/*}*/
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
			
			if (first_shmem && (lump->flags&LUMPFLAG_SHMEM)) {
				/* This is the first element of the
				shmemzied lump list, we can not unlink it!
				It wound corrupt the list otherwise if we
				are in failure_route. -- No problem, only the
				anchor is left in the list */
				
				LOG(L_DBG, "DEBUG: free_rr_lump: lump %p" \
						" is left in the list\n",
						lump);
				
				if (lump->len)
				    LOG(L_CRIT, "BUG: free_rr_lump: lump %p" \
						" can not be removed, but len=%d\n",
						lump, lump->len);
						
				prev_lump=lump;
			} else {
				if (prev_lump) prev_lump->next = lump->next;
				else *list = lump->next;
				if (!(lump->flags&(LUMPFLAG_DUPED|LUMPFLAG_SHMEM)))
					free_lump(lump);
				if (!(lump->flags&LUMPFLAG_SHMEM))
					pkg_free(lump);
			}
		} else {
			/* store previous position */
			prev_lump=lump;
		}
		if (first_shmem && (lump->flags&LUMPFLAG_SHMEM))
			first_shmem=0;
	}
}

#endif
