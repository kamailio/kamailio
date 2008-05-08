/*
 * $Id$
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 * Copyright (C) 2003 August.Net Services, LLC
 * Copyright (C) 2006 Norman Brandinger
 * Copyright (C) 2008 1&1 Internet AG
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
#include <stdio.h>
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../db/db.h"
#include "../../db/db_ut.h"
#include "../../db/db_query.h"
#include "dbase.h"
#include "pg_con.h"
#include "val.h"
#include "res.h"

static int free_query(const db_con_t* _con);


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

db_con_t *db_postgres_init(const str* _url)
{
	return db_do_init(_url, (void*) db_postgres_new_connection);
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

void db_postgres_close(db_con_t* _h)
{
	db_do_close(_h, db_postgres_free_connection);
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

static int db_postgres_submit_query(const db_con_t* _con, const str* _s)
{
	if(! _con || !_s || !_s->s)
	{
		LM_ERR("invalid parameter value\n");
		return(-1);
	}

	/* this bit of nonsense in case our connection get screwed up */
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
			LM_ERR("%p PQstatus(%s) invalid: %.*s\n", _con,
				PQerrorMessage(CON_CONNECTION(_con)), _s->len, _s->s);
			return -1;
	}

	/* free any previous query that is laying about */
	if(CON_RESULT(_con))
	{
		free_query(_con);
	}

	/* exec the query */
	if (PQsendQuery(CON_CONNECTION(_con), _s->s)) {
		LM_DBG("%p PQsendQuery(%.*s)\n", _con, _s->len, _s->s);
	} else {
		LM_ERR("%p PQsendQuery Error: %s Query: %.*s\n", _con,
		PQerrorMessage(CON_CONNECTION(_con)), _s->len, _s->s);
		return -1;
	}

	return 0;
}

/*
 *
 * pg_fetch_result: Gets a partial result set.
 *
 */
int db_postgres_fetch_result(const db_con_t* _con, db_res_t** _res, const int nrows)
{
	int rows;
	PGresult *res = NULL;
	ExecStatusType pqresult;

	if (!_con || !_res || nrows < 0) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/* exit if the fetch count is zero */
	if (nrows == 0) {
		if (*_res)
			db_free_result(*_res);

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
				if (db_postgres_get_columns(_con, *_res) < 0) {
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
					db_free_result(*_res);
        		*_res = 0;
				return 0;
		}

	} else {
		if(RES_ROWS(*_res) != NULL) {
			db_free_rows(*_res);
		}
		RES_ROWS(*_res) = 0;
		RES_ROW_N(*_res) = 0;
	}

	/* Get the number of rows (tuples) in the query result. */
	RES_NUM_ROWS(*_res) = PQntuples(CON_RESULT(_con));

	/* determine the number of rows remaining to be processed */
	rows = RES_NUM_ROWS(*_res) - RES_LAST_ROW(*_res);

	/* If there aren't any more rows left to process, exit */
	if (rows <= 0)
		return 0;

	/* if the fetch count is less than the remaining rows to process                 */
	/* set the number of rows to process (during this call) equal to the fetch count */
	if (nrows < rows)
		rows = nrows;

	RES_ROW_N(*_res) = rows;

	LM_DBG("converting row %d of %d count %d\n", RES_LAST_ROW(*_res),
			RES_NUM_ROWS(*_res), RES_ROW_N(*_res));

	if (db_postgres_convert_rows(_con, *_res) < 0) {
		LM_ERR("failed to convert rows\n");
		if (*_res)
			db_free_result(*_res);

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

static int free_query(const db_con_t* _con)
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
** db_free_result	free the query and free the result memory
**
**	Arguments :
**		db_con_t *	as previously supplied by pg_init()
**		db_res_t *	the result of a query
**
**	Returns :
**		0 upon success
**		negative number upon failure
*/

int db_postgres_free_result(db_con_t* _con, db_res_t* _r)
{
	free_query(_con);
	if (_r) db_free_result(_r);
	_r = 0;

	return 0;
}

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
int db_postgres_query(const db_con_t* _h, const db_key_t* _k, const db_op_t* _op,
	     const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
	     const db_key_t _o, db_res_t** _r)
{
	return db_do_query(_h, _k, _op, _v, _c, _n, _nc, _o, _r, db_postgres_val2str,
		db_postgres_submit_query, db_postgres_store_result);
}


/*
 * Execute a raw SQL query
 */
int db_postgres_raw_query(const db_con_t* _h, const str* _s, db_res_t** _r)
{
	return db_do_raw_query(_h, _s, _r, db_postgres_submit_query,
		db_postgres_store_result);
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

int db_postgres_store_result(const db_con_t* _con, db_res_t** _r)
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
			if (db_postgres_convert_result(_con, *_r) < 0) {
				LM_ERR("%p Error returned from convert_result()\n", _con);
				if (*_r) db_free_result(*_r);

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
        	LM_WARN("%p: %s\n", _con, PQresultErrorMessage(CON_RESULT(_con)));
			if (*_r) db_free_result(*_r);

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
int db_postgres_insert(const db_con_t* _h, const db_key_t* _k, const db_val_t* _v,
		const int _n)
{
	db_res_t* _r = NULL;

	int tmp = db_do_insert(_h, _k, _v, _n, db_postgres_val2str, db_postgres_submit_query);
	// finish the async query, otherwise the next query will not complete
	if (db_postgres_store_result(_h, &_r) != 0)
		LM_WARN("unexpected result returned");
	
	if (_r)
		db_free_result(_r);

	return tmp;
}


/*
 * Delete a row from the specified table
 * _con: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
int db_postgres_delete(const db_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const int _n)
{
	db_res_t* _r = NULL;
	int tmp = db_do_delete(_h, _k, _o, _v, _n, db_postgres_val2str,
		db_postgres_submit_query);

	if (db_postgres_store_result(_h, &_r) != 0)
		LM_WARN("unexpected result returned");
	
	if (_r)
		db_free_result(_r);

	return tmp;
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
int db_postgres_update(const db_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv, const int _n,
		const int _un)
{
	db_res_t* _r = NULL;
	int tmp = db_do_update(_h, _k, _o, _v, _uk, _uv, _n, _un, db_postgres_val2str,
		db_postgres_submit_query);

	if (db_postgres_store_result(_h, &_r) != 0)
		LM_WARN("unexpected result returned");
	
	if (_r)
		db_free_result(_r);

	return tmp;
}


/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_postgres_use_table(db_con_t* _con, const str* _t)
{
	return db_use_table(_con, _t);
}
