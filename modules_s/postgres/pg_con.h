/* 
 * $Id$
 *
 * Portions Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005 iptelorg GmbH
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

#ifndef _PG_CON_H
#define _PG_CON_H

#include "../../db/db_pool.h"
#include "../../db/db_id.h"

#include <time.h>
#include <libpq-fe.h>

enum pg_con_flags {
	PG_INT8_TIMESTAMP = 1 << 0
};

struct pg_con {
	struct db_id* id;        /* Connection identifier */
	unsigned int ref;        /* Reference count */
	struct pool_con* next;   /* Next connection in the pool */

	PGconn* con;             /* Postgres connection handle */
	unsigned int flags;      /* Flags (currently only binary data format) */
	time_t timestamp;        /* Timestamp of last query */
};


/*
 * Some convenience wrappers
 */
#define CON_CONNECTION(db_con) (((struct pg_con*)((db_con)->tail))->con)
#define CON_FLAGS(db_con)      (((struct pg_con*)((db_con)->tail))->flags)
#define CON_TIMESTAMP(db_con)  (((struct pg_con*)((db_con)->tail))->timestamp)


/*
 * Create a new connection structure,
 * open the MySQL connection and set reference count to 1
 */
struct pg_con* pg_new_connection(struct db_id* id);


/*
 * Close the connection and release memory
 */
void pg_free_connection(struct pg_con* con);

#endif /* _PG_CON_H */
