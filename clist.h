/*
 * circular list maintenance macros
 *
 * Copyright (C) 2005 iptelorg GmbH
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
 */

/*!
 * \file
 * \brief Kamailio core :: circular list maintenance macros
 *
 * \author andrei
 * \ingroup core
 * Module: \ref core
 */


#ifndef _clist_h
#define _clist_h

/*! \brief circular list */
#define clist_init(c, next, prev) \
	do{ \
		(c)->next=(void*)(c); \
		(c)->prev=(void*)(c); \
	} while(0)



/*! \brief adds an entire sublist { s,e } (including s & e )
 * after head
 *
 * \note WARNING: clist_insert_sublist(head, n, n->prev) won't work,
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



/*! \brief appends an entire sublist { s,e } (including s & e )
 * at the end of the list
 *
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




/*! \brief remove sublist { s,e } (including s & e )
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



/*! \brief insert after (head) */
#define clist_insert(head, c, next, prev) \
	clist_insert_sublist(head, c, c, next, prev)



/*! \brief  append at the end of the list (head->prev) */
#define clist_append(head, c, next, prev) \
	clist_append_sublist(head, c, c, next, prev)



/*! \brief  remove and element */
#define clist_rm(c, next, prev) \
	clist_rm_sublist(c, c, next, prev)



/*! \brief  iterate on a clist */
#define clist_foreach(head, v, dir) \
	for((v)=(head)->dir; (v)!=(void*)(head); (v)=(v)->dir)

/*! \brief  iterate on a clist, safe version (requires an extra bak. var)
 * (it allows removing of the current element) */
#define clist_foreach_safe(head, v, bak,  dir) \
	for((v)=(head)->dir, (bak)=(v)->dir; (v)!=(void*)(head); \
				(v)=(bak), (bak)=(v)->dir)
#endif
