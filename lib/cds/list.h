/* 
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __LIST_H
#define __LIST_H

#define DOUBLE_LINKED_LIST_ADD(first,last,e)	do { \
		if (last) last->next = (e); \
		else first = (e); \
		(e)->next = NULL; \
		(e)->prev = last; \
		last = (e); \
	} while (0)

#define DOUBLE_LINKED_LIST_REMOVE(first,last,e)	do { \
		if ((e)->next) (e)->next->prev = (e)->prev; \
		else last = (e)->prev; \
		if ((e)->prev) (e)->prev->next = (e)->next; \
		else first = (e)->next; \
		(e)->next = NULL; \
		(e)->prev = NULL; \
	} while (0)

#define LINKED_LIST_ADD(first,last,e)	do { \
		if (last) last->next = (e); \
		else first = (e); \
		(e)->next = NULL; \
		last = (e); \
	} while (0)

#endif
