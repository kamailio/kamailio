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
 */

/**
 * @file 
 * @brief Kamailio core :: Simple str type list and helper functions
 * @ingroup core
 * Module: @ref core
 */

#include "str.h"
#include "mem/mem.h"
#include "str_list.h"


/**
 * @brief Add a new allocated list element to an existing list
 * 
 * Add a new allocated list element to an existing list, the allocation is done
 * from the private memory pool
 * @param s input character
 * @param len length of input character
 * @param last existing list
 * @param total length of total characters in list
 * @return extended list
 */
struct str_list *append_str_list(char *s, int len, struct str_list **last, int *total)
{
	struct str_list *new;
	new = pkg_malloc(sizeof(struct str_list));
	if (!new) {
		PKG_MEM_ERROR;
		return 0;
	}
	new->s.s = s;
	new->s.len = len;
	new->next = 0;

	(*last)->next = new;
	*last = new;
	*total += len;
	return new;
}
