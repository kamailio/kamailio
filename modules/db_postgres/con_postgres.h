/*
 * $Id$
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 *
 * Copyright (C) 2003 August.Net Services, LLC
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
 *
 * ---
 *
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 *
 */


#ifndef CON_POSTGRES_H
#define CON_POSTGRES_H

#include "libpq-fe.h"

/*
 * Postgres specific connection data
 */
struct con_postgres {
	char *sqlurl;	/* the url we are connected to, all connection memory
			   parents from this */
	PGconn *con;	/* this is the postgres connection */
	PGresult *res;	/* this is the current result */
	FILE *fp;	/* debug file output */
	long tpid;	/* record pid of database opener in case one of */
			/* the children try to close the database */
};

#define CON_SQLURL(db_con)    (((struct con_postgres*)((db_con)->tail))->sqlurl)
#define CON_RESULT(db_con)    (((struct con_postgres*)((db_con)->tail))->res)
#define CON_CONNECTION(db_con) (((struct con_postgres*)((db_con)->tail))->con)
#define CON_FP(db_con)        (((struct con_postgres*)((db_con)->tail))->fp)
#define CON_PID(db_con)       (((struct con_postgres*)((db_con)->tail))->tpid)

#define PLOG(f,s) LOG(L_ERR, "PG[%d] %s %s\n",__LINE__,f,s)
#define DLOG(f,s) LOG(L_INFO, "PG[%d] %s %s\n",__LINE__,f,s)

#endif /* CON_POSTGRES_H */
