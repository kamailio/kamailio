/*
 * $Id$
 *
 * here, we delete message lumps which are generated in
 * core functions using pkg_malloc and applied to shmem
 * requests; not doing so would result ugly memory problems
 *
 * I admit it is not a nice hack; -jiri 
 */

#ifndef _FIX_LUMPS_H
#define _FIX_LUMPS_H

/* used to delete attached via lumps from msg; msg can
   be either an original pkg msg, whose Via lump I want
   to delete before generating next branch, or a shmem-stored
   message processed during on_reply -- then I want to
   delete the Via lump for the same reason

   the other case when I want to delete them is when a message
   is stored in shmem for branch picking, forwarded lated and
   Via removal is applied to the shmem-ed message
*/
inline static void free_via_lump( struct lump **list )
{
	struct lump *prev_lump, *lump, *a, *foo;

	prev_lump=0;
	for(lump=*list;lump;lump=lump->next) {
		if (lump->type==HDR_VIA) {
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
