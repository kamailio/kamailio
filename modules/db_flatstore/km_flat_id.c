/* 
 * $Id$
 *
 * Flatstore module connection identifier
 *
 * Copyright (C) 2004 FhG Fokus
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

#include <string.h>
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "km_flat_id.h"



/*
 * Create a new connection identifier
 */
struct flat_id* new_flat_id(char* dir, char* table)
{
	struct flat_id* ptr;
	char* t;
	int t_len;

	if (!dir || !table) {
		LM_ERR("invalid parameter(s)\n");
		return 0;
	}

	ptr = (struct flat_id*)pkg_malloc(sizeof(struct flat_id));
	if (!ptr) {
		LM_ERR("no pkg memory left\n");
		return 0;
	}
	memset(ptr, 0, sizeof(struct flat_id));

	/* alloc memory for the table name */
	t_len = strlen(table);
	t = (char*)pkg_malloc(t_len+1);
	if (!t) {
		LM_ERR("no pkg memory left\n");
		pkg_free(ptr);
		return 0;
	}
	memset(t, 0, t_len);

	ptr->dir.s = dir;
	ptr->dir.len = strlen(dir);
	memcpy(t, table, t_len);
	t[t_len] = '\0';
	ptr->table.s = t;
	ptr->table.len = t_len;

	return ptr;
}


/*
 * Compare two connection identifiers
 */
unsigned char cmp_flat_id(struct flat_id* id1, struct flat_id* id2)
{
	if (!id1 || !id2) return 0;
	if (id1->dir.len != id2->dir.len) return 0;
	if (id1->table.len != id2->table.len) return 0;

	if (memcmp(id1->dir.s, id2->dir.s, id1->dir.len)) return 0;
	if (memcmp(id1->table.s, id2->table.s, id1->table.len)) return 0;
	return 1;
}


/*
 * Free a connection identifier
 */
void free_flat_id(struct flat_id* id)
{
	if (!id) return;
	if (id->table.s)
		pkg_free(id->table.s);
	pkg_free(id);
}
