/*
 * $Id$
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 * Copyright (C) 2003 August.Net Services, LLC
 * Copyright (C) 2006 Norman Brandinger
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
 * 2006-07-28 within pg_get_result(): added check to immediatly return of no 
 *            result set was returned added check to only execute 
 *            convert_result() if PGRES_TUPLES_OK added safety check to avoid 
 *            double pg_free_result() (norm)
 * 2006-08-07 Rewrote pg_get_result().
 *            Additional debugging lines have been placed through out the code.
 *            Added Asynchronous Command Processing (PQsendQuery/PQgetResult) 
 *            instead of PQexec. this was done in preparation of adding FETCH 
 *            support.  Note that PQexec returns a result pointer while 
 *            PQsendQuery does not.  The result set pointer is obtained from 
 *            a call (or multiple calls) to PQgetResult.
 *            Removed transaction processing calls (BEGIN/COMMIT/ROLLBACK) as 
 *            they added uneeded overhead.  Klaus' testing showed in excess of 
 *            1ms gain by removing each command.  In addition, OpenSER only 
 *            issues single queries and is not, at this time transaction aware.
 *            The transaction processing routines have been left in place 
 *            should this support be needed in the future.
 *            Updated logic in pg_query / pg_raw_query to accept a (0) result 
 *            set (_r) parameter.  In this case, control is returned
 *            immediately after submitting the query and no call to 
 *            pg_get_results() is performed. This is a requirement for 
 *            FETCH support. (norm)
 * 2006-10-27 Added fetch support (norm)
 *            Removed dependency on aug_* memory routines (norm)
 *            Added connection pooling support (norm)
 *            Standardized API routines to pg_* names (norm)
 * 2006-11-01 Updated pg_insert(), pg_delete(), pg_update() and 
 *            pg_get_result() to handle failed queries.  Detailed warnings 
 *            along with the text of the failed query is now displayed in the 
 *            log. Callers of these routines can now assume that a non-zero 
 *            rc indicates the query failed and that remedial action may need 
 *            to be taken. (norm)
 */

#define MAXCOLUMNS	512

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../db/db.h"
#include "../../db/db_ut.h"
#include "../../db/db_res.h"
#include "defs.h"
#include "dbase.h"
#include "pg_con.h"

static char _s[SQL_BUF_LEN];

static int submit_query(db_con_t* _con, const char* _s);
static int free_query(db_con_t* _con);

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int pg_use_table(db_con_t* _con, const char* _t)
{

#ifdef PARANOID
	if (!_con) {
		LM_ERR("db_con_t parameter cannot be NULL\n");
		return -1;
	}

	if (!_t) {
		LM_ERR("_t parameter cannot be NULL\n");
		return -1;
	}

#endif

	CON_TABLE(_con) = _t;
	return 0;
}


/*
** pg_init	initialize database for future queries
**
**	Arguments :
**		char *_url;	sql database to open
**
**	Returns :
**		db_con_t * NULL upon error
**		db_con_t * if successful
**
**	Notes :
**		pg_init must be called prior to any database functions.
*/

db_con_t *pg_init(const char* _url)
{
	struct db_id* id;
	struct pg_con* _con;
	db_con_t* _res;
	int con_size = sizeof(db_con_t) + sizeof(struct pg_con*);

	if (strlen(_url)>(SQLURL_LEN-1)) 
	{
		LM_ERR("ERROR sql url too long\n");
		return 0;
	}

	/*
	** this is the root memory for this database connection.
	*/
	_res = (db_con_t*)pkg_malloc(con_size);
	if (!_res) {
		LM_ERR("no more pkg memory for database connection(%i bytes)\n",
				con_size);
		return 0;
	}
	LM_DBG("%p=pkg_malloc(%d) for database connection\n", 
			(db_con_t*)_res, con_size);
	memset(_res, 0, con_size);

	id = new_db_id(_url);
	if (!id) {
		LM_ERR("cannot parse URL '%s'\n", _url);
		goto err;
	}

	/* Find the connection in the pool */
	_con = (struct pg_con*)pool_get(id);
	if (!_con) {
		/* 
		 * The LOG below exposes the username/password of the database connection
		 * by default, it is commented.
		 * LM_DBG("connection '%s' not found in pool\n", _url);
		 *
		 */
		LM_DBG("connection %p not found in pool\n", id);
		_con = pg_new_conn(id);
		if (!_con) {
			LM_ERR("pg_new_con failed to add connection to pool\n");
			goto err;
		}
		pool_insert((struct pool_con*)_con);
	} else {
		/* 
		 * The LOG below exposes the username/password of the database connection
		 * by default, it is commented.
		 * LM_DBG("connection '%s' found in pool\n", _url);
		 *
		 */
		LM_DBG("connection %p found in pool\n", id);
	}

	_res->tail = (unsigned long)_con;
	return _res;

err:
	if (id) free_db_id(id);
	if (_res) {
		LM_ERR("cleaning up: %p=pkg_free()\n", _res);
		pkg_free(_res);
	}
	return 0;
}


/*
** pg_close	last function to call when db is no longer needed
**
**	Arguments :
**		db_con_t * the connection to shut down, as supplied by pg_init()
**
**	Returns :
**		(void)
**
**	Notes :
**		All memory and resources are freed.
*/

void pg_close(db_con_t* _con)
{
	struct pool_con* con;

	con = (struct pool_con*)_con->tail;

	if (pool_remove(con) != 0) {
		pg_free_conn((struct pg_con*)con);
	}
	
	LM_DBG("%p=pkg_free() _con\n", _con);
	pkg_free(_con);
}

/*
** submit_query	run a query
**
**	Arguments :
**		db_con_t *	as previously supplied by pg_init()
**		char *_s	the text query to run
**
**	Returns :
**		0 upon success
**		negative number upon failure
*/

static int submit_query(db_con_t* _con, const char* _s)
{	
#ifdef PARANOID
	if(! _con)
	{
		LM_ERR("db_con_t parameter cannot be NULL\n");
		return(-1);
	}
#endif
	/*
	** this bit of nonsense in case our connection get screwed up 
	*/
	switch(PQstatus(CON_CONNECTION(_con)))
	{
		case CONNECTION_OK: 
			break;
		case CONNECTION_BAD:
			LM_DBG("connection reset\n");
			PQreset(CON_CONNECTION(_con));
			break;
		case CONNECTION_STARTED:
		case CONNECTION_MADE:
		case CONNECTION_AWAITING_RESPONSE:
		case CONNECTION_AUTH_OK:
		case CONNECTION_SETENV:
		case CONNECTION_SSL_STARTUP:
		case CONNECTION_NEEDED:
		default:
                	LM_ERR("%p PQstatus(%s) invalid: %s\n", _con,
							PQerrorMessage(CON_CONNECTION(_con)), _s);
			return -1;
	}

	/*
	** free any previous query that is laying about
	*/
	if(CON_RESULT(_con))
	{
		free_query(_con);
	}

	/*
	** exec the query
	*/

        if (PQsendQuery(CON_CONNECTION(_con), _s)) {
                LM_DBG("%p PQsendQuery(%s)\n", _con, _s);
        } else {
                LM_ERR("%p PQsendQuery Error: %s Query: %s\n", _con,
						PQerrorMessage(CON_CONNECTION(_con)), _s);
		return -1;
        }
	return 0;

}

/*
 *
 * pg_fetch_result: Gets a partial result set.
 *
 */
int pg_fetch_result(db_con_t* _con, db_res_t** _res, int nrows)
{
        int rows;

	PGresult *res = NULL;
        ExecStatusType pqresult;

#ifdef PARANOID
	if (!_con) {
                LM_ERR("db_con_t parameter cannot be NULL\n");
                return -1;
	}
	if (!_res) {
                LM_ERR("db_res_t parameter cannot be NULL\n");
                return -1;
	}
	if (nrows < 0) {
                LM_ERR("nrows parameter cannot be less than zero\n");
                return -1;
	}
#endif

	/* exit if the fetch count is zero */
	if (nrows == 0) {
		if (*_res)
			pg_free_result(*_res);
        	*_res = 0;
		return 0;
	}

	if (*_res == NULL) {
		/* Allocate a new result structure */
		*_res = db_new_result();

		/* Get the result of the previous query */
		while (1) {
			if ((res = PQgetResult(CON_CONNECTION(_con)))) {
				CON_RESULT(_con) = res;
			} else {
				break;
			}
		}
		pqresult = PQresultStatus(CON_RESULT(_con));
		LM_DBG("%p PQresultStatus(%s) PQgetResult(%p)\n", _con,
				PQresStatus(pqresult), CON_RESULT(_con));

        	switch(pqresult) {
                	case PGRES_COMMAND_OK:
                        	/* Successful completion of a command returning no data (such as INSERT or UPDATE). */
                        	return 0;
                	case PGRES_TUPLES_OK:
                        	/* Successful completion of a command returning data (such as a SELECT or SHOW). */
				if (pg_get_columns(_con, *_res) < 0) {
					LM_ERR("failed to get column names\n");
					return -2;
        			}
                        	break;
                	case PGRES_EMPTY_QUERY:
                	case PGRES_COPY_OUT:
                	case PGRES_COPY_IN:
                	case PGRES_BAD_RESPONSE:
                	case PGRES_NONFATAL_ERROR:
                	case PGRES_FATAL_ERROR:
        				LM_WARN("%p - probable invalid query\n", _con);
                	default:
        				LM_WARN("%p - PQresultStatus(%s)\n",
							_con, PQresStatus(pqresult));
       				if (*_res) 
					pg_free_result(*_res);
        			*_res = 0;
                        	return 0;
        	}

	} else {
                if(RES_ROWS(*_res) != NULL) {
                        pg_free_rows(*_res);
		}
		RES_ROW_N(*_res) = 0;
	}

	/* determine the number of rows remaining to be processed */
	rows = RES_NUM_ROWS(*_res) - RES_LAST_ROW(*_res);

	/* If there aren't any more rows left to process, exit */
        if (rows <= 0)
                return 0;
	
	/* if the fetch count is less than the remaining rows to process		 */
	/* set the number of rows to process (during this call) equal to the fetch count */
        if (nrows < rows)
               rows = nrows;

        RES_ROW_N(*_res) = rows;

	LM_DBG("converting row %d of %d count %d\n", RES_LAST_ROW(*_res),
			RES_NUM_ROWS(*_res), RES_ROW_N(*_res));

        if (pg_convert_rows(_con, *_res, RES_LAST_ROW(*_res), RES_ROW_N(*_res)) < 0) {
                LM_ERR("failed to convert rows\n");
		if (*_res) 
                	pg_free_result(*_res);
        	*_res = 0;
		return -3;
        }

	/* update the total number of rows processed */
        RES_LAST_ROW(*_res) += rows;

        return 0;
}

/*
** free_query	clear the db channel and clear any old query result status
**
**	Arguments :
**		db_con_t *	as previously supplied by pg_init()
**
**	Returns :
**		0 upon success
**		negative number upon failure
*/

static int free_query(db_con_t* _con)
{
	if(CON_RESULT(_con))
	{
		LM_DBG("PQclear(%p) result set\n", CON_RESULT(_con));
		PQclear(CON_RESULT(_con));
		CON_RESULT(_con) = 0;
	}

	return 0;
}

/*
** db_free_query	free the query and free the result memory
**
**	Arguments :
**		db_con_t *	as previously supplied by pg_init()
**		db_res_t *	the result of a query
**
**	Returns :
**		0 upon success
**		negative number upon failure
*/

int pg_free_query(db_con_t* _con, db_res_t* _r)
{
	free_query(_con);
        if (_r) pg_free_result(_r);
        _r = 0;

	return 0;
}

#if 0
/*
** begin_transaction	begin transaction
**
**	Arguments :
**		db_con_t *	as previously supplied by pg_init()
**		char *		this is just in case an error message
**				is printed, we will know which query
**				was going to be run, giving us a code debug hint
**
**	Returns :
**		0 upon success
**		negative number upon failure
**
**	Notes :
**		This function may be called with a messed up communication
**		channel.  Therefore, alot of this function is dealing with
**		that.  Wen the layering gets corrected later this stuff
**		should continue to work correctly, it will just be
**		way to defensive.
*/

static int begin_transaction(db_con_t * _con, char *_s)
{
	PGresult *mr;
	int rv;

	/*
	** Note:
	**  The upper layers of code may attempt a transaction
	**  before opening or having a valid connection to the
	**  database.  We try to sense this, and open the database
	**  if we have the sqlurl in the _con structure.  Otherwise,
	**  all we can do is return an error.
	*/

	if(_con)
	{
		if(CON_CONNECTED(_con))
		{
			mr = PQexec(CON_CONNECTION(_con), "BEGIN");
			if(!mr || PQresultStatus(mr) != PGRES_COMMAND_OK)
			{
				/*
				** We get here if the connection to the
				** db is corrupt, which can happen a few
				** different ways, but all of them are
				** related to the parent process forking,
				** or being forked.
				*/
				LM_ERR("corrupt connection\n");
				CON_CONNECTED(_con) = 0;
			}
			else
			{
				/*
				** this is the normal way out.
				** the transaction ran fine.
				*/
				LM_DBG("%p PQclear(%p) result set\n", _con, mr);
				PQclear(mr);
				return(0);
			}
		}
		else
		{
			LM_DBG("called before pg_init\n");
		}

		/*
		** if we get here we have a corrupt db connection,
		** but we probably have a valid db_con_t structure.
		** attempt to open the db.
		*/

		if((rv = connect_db(_con)) != 0)
		{
			/*
			** our attempt to fix the connection failed
			*/
			char buf[SQL_BUF_LEN];
			snprintf(buf, SQL_BUF_LEN, "no connection, FATAL %d!", rv);
			LM_DBG("%s\n", buf);
			return(rv);
		}
		LM_DBG("successfully reconnected\n");
	}
	else
	{
		LM_DBG("must call pg_init first!\n");
		return(-1);
	}

	/*
	** we get here if the database connection was corrupt,
	** i didn't want to use recursion ...
	*/

	mr = PQexec(CON_CONNECTION(_con), "BEGIN");
	if(!mr || PQresultStatus(mr) != PGRES_COMMAND_OK)
	{
		char buf[SQL_BUF_LEN];
		snprintf(buf, SQL_BUF_LEN, "FATAL %s, '%s'!\n",
			PQerrorMessage(CON_CONNECTION(_con)), _s);
		LM_DBG("%s\n", buf);
		return(-1);
	}

	LM_DBG("db channel reset successful\n");

	LM_DBG("%p PQclear(%p) result set\n", _con, mr);
	PQclear(mr);
	return(0);
}

/*
** commit_transaction	any begin_transaction must be terminated with this
**
**	Arguments :
**		db_con_t *	as previously supplied by pg_init()
**
**	Returns :
**		0 upon success
**		negative number upon failure
*/

static int commit_transaction(db_con_t * _con)
{
	PGresult *mr;

	mr = PQexec(CON_CONNECTION(_con), "COMMIT");
	if(!mr || PQresultStatus(mr) != PGRES_COMMAND_OK)
	{
		LM_ERR("error");
		return -1;
	}
	LM_DBG("%p PQclear(%p) result set\n", _con, mr);
	PQclear(mr);
	return(0);
}

/*
** rollback_transaction:	any failed begin_transaction must be terminated with this
**
**	Arguments :
**		db_con_t *	as previously supplied by pg_init()
**
**	Returns :
**		0 upon success
**		negative number upon failure
*/

static int rollback_transaction(db_con_t * _con)
{
	PGresult *mr;

	mr = PQexec(CON_CONNECTION(_con), "ROLLBACK");
	if(!mr || PQresultStatus(mr) != PGRES_COMMAND_OK)
	{
		LM_ERR("error");
		return -1;
	}
	LM_DBG("%p PQclear(%p) result set\n", _con, mr);
	PQclear(mr);
	return(0);
}

#endif



/*
 * Query table for specified rows
 * _con: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: nmber of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 */
int pg_query(db_con_t* _con, db_key_t* _k, db_op_t* _op,
	     db_val_t* _v, db_key_t* _c, int _n, int _nc,
	     db_key_t _o, db_res_t** _r)
{
	int off, rv;
	if (!_c) {
		off = snprintf(_s, SQL_BUF_LEN,
			"select * from %s ", CON_TABLE(_con));
	} else {
		off = snprintf(_s, SQL_BUF_LEN, "select ");
		off += db_print_columns(_s + off, SQL_BUF_LEN - off, _c, _nc);
		off += snprintf(_s + off, SQL_BUF_LEN - off,
			"from %s ", CON_TABLE(_con));
	}
	if (_n) {
		off += snprintf(_s + off, SQL_BUF_LEN - off, "where ");
		off += db_print_where(_con, _s + off, SQL_BUF_LEN - off,
			_k, _op, _v, _n, val2str);
	}
	if (_o) {
		off += snprintf(_s + off, SQL_BUF_LEN - off,
			" order by %s", _o);
	}

	LM_DBG("%p %p %s\n", _con, _r, _s);

        if(_r) {
		/* if(begin_transaction(_con, _s)) return(-1); */
		if (submit_query(_con, _s) < 0) {
			LM_ERR("failed to submit query\n");
			/* rollback_transaction(_con); */
			return -2;
		}
		rv = pg_get_result(_con, _r);
		/* commit_transaction(_con); */
		return(rv);
	} else {
		if (submit_query(_con, _s) < 0) {
			LM_ERR("failed to submit query\n");
			return -2;
		}
	}

	return 0;
}


/*
 * Execute a raw SQL query
 */
int pg_raw_query(db_con_t* _con, char* _s, db_res_t** _r)
{
	int rv;
	
	LM_DBG("%p %p %s\n", _con, _r, _s);

        if(_r) {
                /* if(begin_transaction(_con, _s)) return(-1); */
                if (submit_query(_con, _s) < 0) {
						LM_ERR("failed to submit query\n");
                        /* rollback_transaction(_con); */
                        return -2;
                }
                rv = pg_get_result(_con, _r);
                /* commit_transaction(_con); */
                return(rv);
        } else {
                if (submit_query(_con, _s) < 0) {
						LM_ERR("failed to submit query\n");
                        return -2;
                }
        }

        return 0;

}

/*
 * Retrieve result set
 *
 * Input:
 *   db_con_t*  _con Structure representing the database connection
 *   db_res_t** _r pointer to a structure represending the result set
 *
 * Output:
 *   return 0: If the status of the last command produced a result set and,
 *   If the result set contains data or the convert_result() routine
 *   completed successfully.
 *
 *   return < 0: If the status of the last command was not handled or if the
 *   convert_result() returned an error.
 *
 * Notes:
 *   A new result structure is allocated on every call to this routine.
 *
 *   If this routine returns 0, it is the callers responsbility to free the
 *   result structure. If this routine returns < 0, then the result structure
 *   is freed before returning to the caller.
 *
 */

int pg_get_result(db_con_t* _con, db_res_t** _r)
{
	PGresult *res = NULL;
        ExecStatusType pqresult;
	int rc = 0;

	*_r = db_new_result();

	while (1) {
		if ((res = PQgetResult(CON_CONNECTION(_con)))) {
			CON_RESULT(_con) = res;
		} else {
			break;
		}
	}

        pqresult = PQresultStatus(CON_RESULT(_con));
	
	LM_DBG("%p PQresultStatus(%s) PQgetResult(%p)\n", _con,
			PQresStatus(pqresult), CON_RESULT(_con));

        switch(pqresult) {
                case PGRES_COMMAND_OK:
                        /* Successful completion of a command returning no data (such as INSERT or UPDATE). */
                        rc = 0;
			break;
                case PGRES_TUPLES_OK:
                        /* Successful completion of a command returning data (such as a SELECT or SHOW). */
                        if (pg_convert_result(_con, *_r) < 0) {
                                LM_ERR("%p Error returned from"
										"convert_result()\n", _con);
       				if (*_r) pg_free_result(*_r);
        			*_r = 0;
                                rc = -4;
                        }
                        rc =  0;
                        break;
                case PGRES_EMPTY_QUERY:
                case PGRES_COPY_OUT:
                case PGRES_COPY_IN:
                case PGRES_BAD_RESPONSE:
                case PGRES_NONFATAL_ERROR:
                case PGRES_FATAL_ERROR:
        		LM_WARN("%p Probable invalid query\n", _con);
                default:
        		LM_WARN("%p: %s\n", _con, PQresStatus(pqresult));
        		LM_WARN("%p: %s\n", _con,
						PQresultErrorMessage(CON_RESULT(_con)));
       			if (*_r) pg_free_result(*_r);
        		*_r = 0;
			rc = (int)pqresult;
			break;
        }

	free_query(_con);
        return (rc);
}

/*
 * Insert a row into specified table
 * _con: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
int pg_insert(db_con_t* _con, db_key_t* _k, db_val_t* _v, int _n)
{
	db_res_t* _r = NULL;
	int off;
	int rv = 0;

	off = snprintf(_s, SQL_BUF_LEN, "insert into %s (", CON_TABLE(_con));
	off += db_print_columns(_s + off, SQL_BUF_LEN - off, _k, _n);
	off += snprintf(_s + off, SQL_BUF_LEN - off, ") values (");
	off += db_print_values(_con, _s + off, SQL_BUF_LEN - off, _v, _n, val2str);
	*(_s + off++) = ')';
	*(_s + off) = '\0';

	LM_DBG("%p %s\n", _con, _s);

	/* if(begin_transaction(_con, _s)) return(-1); */
	if (submit_query(_con, _s) < 0) {
		LM_ERR("failed to insert\n");
		/* rollback_transaction(_con); */
		return -2;
	}
	rv = pg_get_result(_con,&_r);	
	if (rv != 0) {
		LM_WARN("%p Query: %s\n", _con, _s);
	}
	if (_r) 
		pg_free_result(_r);
	/* commit_transaction(_con); */
	return(rv);
}


/*
 * Delete a row from the specified table
 * _con: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
int pg_delete(db_con_t* _con, db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n)
{
	db_res_t* _r = NULL;
	int off;
	int rv = 0;

	off = snprintf(_s, SQL_BUF_LEN, "delete from %s", CON_TABLE(_con));
	if (_n) {
		off += snprintf(_s + off, SQL_BUF_LEN - off, " where ");
		off += db_print_where(_con, _s + off, SQL_BUF_LEN - off, _k,
			_o, _v, _n, val2str);
	}

	LM_DBG("%p %s\n", _con, _s);

	/* if(begin_transaction(_con, _s)) return(-1); */
	if (submit_query(_con, _s) < 0) {
		LM_ERR("failed to delete\n");
		/* rollback_transaction(_con); */
		return -2;
	}
	rv = pg_get_result(_con,&_r);	
	if (rv != 0) {
		LM_WARN("%p Query: %s\n", _con, _s);
	}
	if (_r) 
		pg_free_result(_r);
	/* commit_transaction(_con); */
	return(rv);
}


/*
 * Update some rows in the specified table
 * _con: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _uk: updated columns
 * _uv: updated values of the columns
 * _n: number of key=value pairs
 * _un: number of columns to update
 */
int pg_update(db_con_t* _con, db_key_t* _k, db_op_t* _o, db_val_t* _v,
	      db_key_t* _uk, db_val_t* _uv, int _n, int _un)
{
	db_res_t* _r = NULL;
	int off;
	int rv = 0;

	off = snprintf(_s, SQL_BUF_LEN, "update %s set ", CON_TABLE(_con));
	off += db_print_set(_con, _s + off, SQL_BUF_LEN - off, _uk, _uv, _un, val2str);
	if (_n) {
		off += snprintf(_s + off, SQL_BUF_LEN - off, " where ");
		off += db_print_where(_con, _s + off, SQL_BUF_LEN - off, _k,
			_o, _v, _n, val2str);
		*(_s + off) = '\0';
	}

	LM_DBG("%p %s\n", _con, _s);

	/* if(begin_transaction(_con, _s)) return(-1); */
	if (submit_query(_con, _s) < 0) {
		LM_ERR("failed to update\n");
		/* rollback_transaction(_con); */
		return -2;
	}
	rv = pg_get_result(_con,&_r);	
	if (rv != 0) {
		LM_WARN("%p Query: %s\n", _con, _s);
	}
	if (_r) 
		pg_free_result(_r);
	/* commit_transaction(_con); */
	return(rv);
}
