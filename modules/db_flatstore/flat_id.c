/* 
 * $Id$
 *
 * Flatstore module connection identifier
 *
 * Copyright (C) 2004 FhG Fokus
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

#include <string.h>
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "flat_id.h"



/*
 * Create a new connection identifier
 */
struct flat_id* new_flat_id(char* dir, char* table)
{
	struct flat_id* ptr;

	if (!dir || !table) {
		LOG(L_ERR, "new_flat_id: Invalid parameter(s)\n");
		return 0;
	}

	ptr = (struct flat_id*)pkg_malloc(sizeof(struct flat_id));
	if (!ptr) {
		LOG(L_ERR, "new_flat_id: No memory left\n");
		return 0;
	}
	memset(ptr, 0, sizeof(struct flat_id));

	ptr->dir.s = dir;
	ptr->dir.len = strlen(dir);
	ptr->table.s = table;
	ptr->table.len = strlen(table);

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
	pkg_free(id);
}
