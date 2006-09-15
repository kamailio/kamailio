/*
 * $Id$
 *
 * circular list maintenance macros
 *
 * Copyright (C) 2005 iptelorg GmbH
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

/* History:
 * --------
 *  2005-08-08  created by andrei
 */

#ifndef _clist_h
#define _clist_h

/* circular list */
#define clist_init(c, next, prev) \
	do{ \
		(c)->next=(void*)(c); \
		(c)->prev=(void*)(c); \
	} while(0)



/* adds an entire sublist { s,e } (including s & e )
 * after head
 * WARNING: clist_insert_sublist(head, n, n->prev) won't work,
 *          same for clist_insert_sublist(head, n->next, n)
 *  (macro!), use  e=n->prev; clist_insert_sublist(head, n, e, ...)
 *  instead!
 */
#define clist_insert_sublist(head, s, e, next, prev) \
	do{ \
		(s)->prev=(void*)(head); \
		(e)->next=(head)->next; \
		(e)->next->prev=(e); \
		(head)->next=s;   \
	}while(0)



/* appends an entire sublist { s,e } (including s & e )
 * at the end of the list
 * WARNING: clist_append_sublist(head, n, n->prev, ...) won't work,
 *  (macro!), use  e=n->prev; clist_append_sublist(head, n, e, ...)
 *  instead!
 */
#define clist_append_sublist(head, s, e, next, prev) \
	do{ \
		(s)->prev=(head)->prev; \
		(e)->next=(void*)(head); \
		(s)->prev->next=(s); \
		(head)->prev=(e);   \
	}while(0)




/* remove sublist { s,e } (including s & e )
 * always, if start is the beginning of the list use
 * clist_rm_sublist(head->next, e, next, prev )
 * WARNING: clist_rm_sublist(n, n->prev, ...) won't work,
 *  (macro!), use  e=n->prev; clist_rm_sublist(n, e, ...)
 *  instead! */
#define clist_rm_sublist(s, e, next, prev) \
	do{\
		(s)->prev->next=(e)->next;  \
		(e)->next->prev=(s)->prev ; \
	}while(0)



/* insert after (head) */
#define clist_insert(head, c, next, prev) \
	clist_insert_sublist(head, c, c, next, prev)



/* append at the end of the list (head->prev) */
#define clist_append(head, c, next, prev) \
	clist_append_sublist(head, c, c, next, prev)



/* remove and element */
#define clist_rm(c, next, prev) \
	clist_rm_sublist(c, c, next, prev)



/* iterate on a clist */
#define clist_foreach(head, v, dir) \
	for((v)=(head)->dir; (v)!=(void*)(head); (v)=(v)->dir)

/* iterate on a clist, safe version (requires an extra bak. var)
 * (it allows removing of the current element) */
#define clist_foreach_safe(head, v, bak,  dir) \
	for((v)=(head)->dir, (bak)=(v)->dir; (v)!=(void*)(head); \
				(v)=(bak), (bak)=(v)->dir)
#endif
