/* 
 * $Id$
 *
 * Flatstore connection identifier
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

#ifndef _KM_FLAT_ID_H
#define _KM_FLAT_ID_H

#include "../../str.h"


struct flat_id {
	str dir;   /* Database directory */ 
	str table; /* Name of table */
};


/*
 * Create a new connection identifier
 */
struct flat_id* new_flat_id(char* dir, char* table);


/*
 * Compare two connection identifiers
 */
unsigned char cmp_flat_id(struct flat_id* id1, struct flat_id* id2);


/*
 * Free a connection identifier
 */
void free_flat_id(struct flat_id* id);


#endif /* _KM_FLAT_ID_H */
