/* 
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2008 1&1 Internet AG
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

/*! \file
 *  \brief DB_MYSQL :: Core
 *  \ref my_con.c
 *  \ingroup db_mysql
 *  Module: \ref db_mysql
 */


#ifndef KM_MY_CON_H
#define KM_MY_CON_H

#include "../../lib/srdb1/db_pool.h"
#include "../../lib/srdb1/db_id.h"

#include <time.h>
#include <mysql/mysql.h>


struct my_con {
	struct db_id* id;        /*!< Connection identifier */
	unsigned int ref;        /*!< Reference count */
	struct pool_con* next;   /*!< Next connection in the pool */

	MYSQL* con;              /*!< Connection representation */
	time_t timestamp;        /*!< Timestamp of last query */
	int transaction;         /*!< Multi-query transaction is currently open */
	int lockedtables;        /*!< Table locks were aquired */
};


/*
 * Some convenience wrappers
 */
#define CON_CONNECTION(db_con)  (((struct my_con*)((db_con)->tail))->con)
#define CON_TIMESTAMP(db_con)   (((struct my_con*)((db_con)->tail))->timestamp)
#define CON_TRANSACTION(db_con) (((struct my_con*)((db_con)->tail))->transaction)
#define CON_LOCKEDTABLES(db_con) (((struct my_con*)((db_con)->tail))->lockedtables)


/*! \brief
 * Create a new connection structure,
 * open the MySQL connection and set reference count to 1
 */
struct my_con* db_mysql_new_connection(const struct db_id* id);


/*! \brief
 * Close the connection and release memory
 */
void db_mysql_free_connection(struct pool_con* con);

#endif /* KM_MY_CON_H */
