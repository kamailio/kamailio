/* 
 * $Id$
 *
 * Flatstore module connection structure
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

#ifndef _FLAT_CON_H
#define _FLAT_CON_H

#include <stdio.h>
#include <time.h>
#include "flat_id.h"

struct flat_con {
	struct flat_id* id;    /* Connection identifier */
	int ref;               /* Reference count */
	FILE* file;            /* File descriptor structure */
	struct flat_con* next; /* Next connection in the pool */
};


/*
 * Some convenience wrappers
 */
#define CON_FILE(db_con) (((struct flat_con*)((db_con)->tail))->file)


/*
 * Create a new connection structure,
 * open the MySQL connection and set reference count to 1
 */
struct flat_con* flat_new_connection(struct flat_id* id);


/*
 * Close the connection and release memory
 */
void flat_free_connection(struct flat_con* con);


#endif /* _FLAT_CON_H */
